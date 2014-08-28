// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sandboxed_unpacker.h"

#include <set>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/file_util_proxy.h"
#include "base/files/scoped_file.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/constants.h"
#include "components/crx_file/crx_file.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/common/common_param_traits.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

using base::ASCIIToUTF16;
using content::BrowserThread;
using content::UtilityProcessHost;
using crx_file::CrxFile;

// The following macro makes histograms that record the length of paths
// in this file much easier to read.
// Windows has a short max path length. If the path length to a
// file being unpacked from a CRX exceeds the max length, we might
// fail to install. To see if this is happening, see how long the
// path to the temp unpack directory is. See crbug.com/69693 .
#define PATH_LENGTH_HISTOGRAM(name, path) \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, path.value().length(), 0, 500, 100)

// Record a rate (kB per second) at which extensions are unpacked.
// Range from 1kB/s to 100mB/s.
#define UNPACK_RATE_HISTOGRAM(name, rate) \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, rate, 1, 100000, 100);

namespace extensions {
namespace {

void RecordSuccessfulUnpackTimeHistograms(
    const base::FilePath& crx_path, const base::TimeDelta unpack_time) {

  const int64 kBytesPerKb = 1024;
  const int64 kBytesPerMb = 1024 * 1024;

  UMA_HISTOGRAM_TIMES("Extensions.SandboxUnpackSuccessTime", unpack_time);

  // To get a sense of how CRX size impacts unpack time, record unpack
  // time for several increments of CRX size.
  int64 crx_file_size;
  if (!base::GetFileSize(crx_path, &crx_file_size)) {
    UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccessCantGetCrxSize", 1);
    return;
  }

  // Cast is safe as long as the number of bytes in the CRX is less than
  // 2^31 * 2^10.
  int crx_file_size_kb = static_cast<int>(crx_file_size / kBytesPerKb);
  UMA_HISTOGRAM_COUNTS(
      "Extensions.SandboxUnpackSuccessCrxSize", crx_file_size_kb);

  // We have time in seconds and file size in bytes.  We want the rate bytes are
  // unpacked in kB/s.
  double file_size_kb =
      static_cast<double>(crx_file_size) / static_cast<double>(kBytesPerKb);
  int unpack_rate_kb_per_s =
      static_cast<int>(file_size_kb / unpack_time.InSecondsF());
  UNPACK_RATE_HISTOGRAM("Extensions.SandboxUnpackRate", unpack_rate_kb_per_s);

  if (crx_file_size < 50.0 * kBytesPerKb) {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRateUnder50kB", unpack_rate_kb_per_s);

  } else if (crx_file_size < 1 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRate50kBTo1mB", unpack_rate_kb_per_s);

  } else if (crx_file_size < 2 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRate1To2mB", unpack_rate_kb_per_s);

  } else if (crx_file_size < 5 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRate2To5mB", unpack_rate_kb_per_s);

  } else if (crx_file_size < 10 * kBytesPerMb) {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRate5To10mB", unpack_rate_kb_per_s);

  } else {
    UNPACK_RATE_HISTOGRAM(
        "Extensions.SandboxUnpackRateOver10mB", unpack_rate_kb_per_s);
  }
}

// Work horse for FindWritableTempLocation. Creates a temp file in the folder
// and uses NormalizeFilePath to check if the path is junction free.
bool VerifyJunctionFreeLocation(base::FilePath* temp_dir) {
  if (temp_dir->empty())
    return false;

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(*temp_dir, &temp_file)) {
    LOG(ERROR) << temp_dir->value() << " is not writable";
    return false;
  }
  // NormalizeFilePath requires a non-empty file, so write some data.
  // If you change the exit points of this function please make sure all
  // exit points delete this temp file!
  if (base::WriteFile(temp_file, ".", 1) != 1)
    return false;

  base::FilePath normalized_temp_file;
  bool normalized = base::NormalizeFilePath(temp_file, &normalized_temp_file);
  if (!normalized) {
    // If |temp_file| contains a link, the sandbox will block al file system
    // operations, and the install will fail.
    LOG(ERROR) << temp_dir->value() << " seem to be on remote drive.";
  } else {
    *temp_dir = normalized_temp_file.DirName();
  }
  // Clean up the temp file.
  base::DeleteFile(temp_file, false);

  return normalized;
}

// This function tries to find a location for unpacking the extension archive
// that is writable and does not lie on a shared drive so that the sandboxed
// unpacking process can write there. If no such location exists we can not
// proceed and should fail.
// The result will be written to |temp_dir|. The function will write to this
// parameter even if it returns false.
bool FindWritableTempLocation(const base::FilePath& extensions_dir,
                              base::FilePath* temp_dir) {
// On ChromeOS, we will only attempt to unpack extension in cryptohome (profile)
// directory to provide additional security/privacy and speed up the rest of
// the extension install process.
#if !defined(OS_CHROMEOS)
  PathService::Get(base::DIR_TEMP, temp_dir);
  if (VerifyJunctionFreeLocation(temp_dir))
    return true;
#endif

  *temp_dir = file_util::GetInstallTempDir(extensions_dir);
  if (VerifyJunctionFreeLocation(temp_dir))
    return true;
  // Neither paths is link free chances are good installation will fail.
  LOG(ERROR) << "Both the %TEMP% folder and the profile seem to be on "
             << "remote drives or read-only. Installation can not complete!";
  return false;
}

// Read the decoded images back from the file we saved them to.
// |extension_path| is the path to the extension we unpacked that wrote the
// data. Returns true on success.
bool ReadImagesFromFile(const base::FilePath& extension_path,
                        DecodedImages* images) {
  base::FilePath path =
      extension_path.AppendASCII(kDecodedImagesFilename);
  std::string file_str;
  if (!base::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  PickleIterator iter(pickle);
  return IPC::ReadParam(&pickle, &iter, images);
}

// Read the decoded message catalogs back from the file we saved them to.
// |extension_path| is the path to the extension we unpacked that wrote the
// data. Returns true on success.
bool ReadMessageCatalogsFromFile(const base::FilePath& extension_path,
                                 base::DictionaryValue* catalogs) {
  base::FilePath path = extension_path.AppendASCII(
      kDecodedMessageCatalogsFilename);
  std::string file_str;
  if (!base::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  PickleIterator iter(pickle);
  return IPC::ReadParam(&pickle, &iter, catalogs);
}

}  // namespace

SandboxedUnpacker::SandboxedUnpacker(
    const base::FilePath& crx_path,
    Manifest::Location location,
    int creation_flags,
    const base::FilePath& extensions_dir,
    const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
    SandboxedUnpackerClient* client)
    : crx_path_(crx_path),
      client_(client),
      extensions_dir_(extensions_dir),
      got_response_(false),
      location_(location),
      creation_flags_(creation_flags),
      unpacker_io_task_runner_(unpacker_io_task_runner) {
}

bool SandboxedUnpacker::CreateTempDirectory() {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());

  base::FilePath temp_dir;
  if (!FindWritableTempLocation(extensions_dir_, &temp_dir)) {
    ReportFailure(
        COULD_NOT_GET_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_GET_TEMP_DIRECTORY")));
    return false;
  }

  if (!temp_dir_.CreateUniqueTempDirUnderPath(temp_dir)) {
    ReportFailure(
        COULD_NOT_CREATE_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_CREATE_TEMP_DIRECTORY")));
    return false;
  }

  return true;
}

void SandboxedUnpacker::Start() {
  // We assume that we are started on the thread that the client wants us to do
  // file IO on.
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());

  unpack_start_time_ = base::TimeTicks::Now();

  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackInitialCrxPathLength",
                        crx_path_);
  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  // Initialize the path that will eventually contain the unpacked extension.
  extension_root_ = temp_dir_.path().AppendASCII(kTempExtensionName);
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackUnpackedCrxPathLength",
                        extension_root_);

  // Extract the public key and validate the package.
  if (!ValidateSignature())
    return;  // ValidateSignature() already reported the error.

  // Copy the crx file into our working directory.
  base::FilePath temp_crx_path = temp_dir_.path().Append(crx_path_.BaseName());
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackTempCrxPathLength",
                        temp_crx_path);

  if (!base::CopyFile(crx_path_, temp_crx_path)) {
    // Failed to copy extension file to temporary directory.
    ReportFailure(
        FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY")));
    return;
  }

  // The utility process will have access to the directory passed to
  // SandboxedUnpacker.  That directory should not contain a symlink or NTFS
  // reparse point.  When the path is used, following the link/reparse point
  // will cause file system access outside the sandbox path, and the sandbox
  // will deny the operation.
  base::FilePath link_free_crx_path;
  if (!base::NormalizeFilePath(temp_crx_path, &link_free_crx_path)) {
    LOG(ERROR) << "Could not get the normalized path of "
               << temp_crx_path.value();
    ReportFailure(
        COULD_NOT_GET_SANDBOX_FRIENDLY_PATH,
        l10n_util::GetStringUTF16(IDS_EXTENSION_UNPACK_FAILED));
    return;
  }
  PATH_LENGTH_HISTOGRAM("Extensions.SandboxUnpackLinkFreeCrxPathLength",
                        link_free_crx_path);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SandboxedUnpacker::StartProcessOnIOThread,
          this,
          link_free_crx_path));
}

SandboxedUnpacker::~SandboxedUnpacker() {
}

bool SandboxedUnpacker::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SandboxedUnpacker, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackExtension_Succeeded,
                        OnUnpackExtensionSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackExtension_Failed,
                        OnUnpackExtensionFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SandboxedUnpacker::OnProcessCrashed(int exit_code) {
  // Don't report crashes if they happen after we got a response.
  if (got_response_)
    return;

  // Utility process crashed while trying to install.
  ReportFailure(
     UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,
     l10n_util::GetStringFUTF16(
         IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
         ASCIIToUTF16("UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL")) +
       ASCIIToUTF16(". ") +
       l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_PROCESS_CRASHED));
}

void SandboxedUnpacker::StartProcessOnIOThread(
    const base::FilePath& temp_crx_path) {
  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, unpacker_io_task_runner_.get());
  // Grant the subprocess access to the entire subdir the extension file is
  // in, so that it can unpack to that dir.
  host->SetExposedDir(temp_crx_path.DirName());
  host->Send(
      new ChromeUtilityMsg_UnpackExtension(
          temp_crx_path, extension_id_, location_, creation_flags_));
}

void SandboxedUnpacker::OnUnpackExtensionSucceeded(
    const base::DictionaryValue& manifest) {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  got_response_ = true;

  scoped_ptr<base::DictionaryValue> final_manifest(
      RewriteManifestFile(manifest));
  if (!final_manifest)
    return;

  // Create an extension object that refers to the temporary location the
  // extension was unpacked to. We use this until the extension is finally
  // installed. For example, the install UI shows images from inside the
  // extension.

  // Localize manifest now, so confirm UI gets correct extension name.

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with base::string16
  std::string utf8_error;
  if (!extension_l10n_util::LocalizeExtension(extension_root_,
                                              final_manifest.get(),
                                              &utf8_error)) {
    ReportFailure(
        COULD_NOT_LOCALIZE_EXTENSION,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
            base::UTF8ToUTF16(utf8_error)));
    return;
  }

  extension_ = Extension::Create(
      extension_root_,
      location_,
      *final_manifest,
      Extension::REQUIRE_KEY | creation_flags_,
      &utf8_error);

  if (!extension_.get()) {
    ReportFailure(INVALID_MANIFEST,
                  ASCIIToUTF16("Manifest is invalid: " + utf8_error));
    return;
  }

  SkBitmap install_icon;
  if (!RewriteImageFiles(&install_icon))
    return;

  if (!RewriteCatalogFiles())
    return;

  ReportSuccess(manifest, install_icon);
}

void SandboxedUnpacker::OnUnpackExtensionFailed(const base::string16& error) {
  CHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  got_response_ = true;
  ReportFailure(
      UNPACKER_CLIENT_FAILED,
      l10n_util::GetStringFUTF16(
           IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
           error));
}

bool SandboxedUnpacker::ValidateSignature() {
  base::ScopedFILE file(base::OpenFile(crx_path_, "rb"));

  if (!file.get()) {
    // Could not open crx file for reading.
#if defined (OS_WIN)
    // On windows, get the error code.
    uint32 error_code = ::GetLastError();
    // TODO(skerner): Use this histogram to understand why so many
    // windows users hit this error.  crbug.com/69693

    // Windows errors are unit32s, but all of likely errors are in
    // [1, 1000].  See winerror.h for the meaning of specific values.
    // Clip errors outside the expected range to a single extra value.
    // If there are errors in that extra bucket, we will know to expand
    // the range.
    const uint32 kMaxErrorToSend = 1001;
    error_code = std::min(error_code, kMaxErrorToSend);
    UMA_HISTOGRAM_ENUMERATION("Extensions.ErrorCodeFromCrxOpen",
                              error_code, kMaxErrorToSend);
#endif

    ReportFailure(
        CRX_FILE_NOT_READABLE,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_FILE_NOT_READABLE")));
    return false;
  }

  // Read and verify the header.
  // TODO(erikkay): Yuck.  I'm not a big fan of this kind of code, but it
  // appears that we don't have any endian/alignment aware serialization
  // code in the code base.  So for now, this assumes that we're running
  // on a little endian machine with 4 byte alignment.
  CrxFile::Header header;
  size_t len = fread(&header, 1, sizeof(header), file.get());
  if (len < sizeof(header)) {
    // Invalid crx header
    ReportFailure(
        CRX_HEADER_INVALID,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_HEADER_INVALID")));
    return false;
  }

  CrxFile::Error error;
  scoped_ptr<CrxFile> crx(CrxFile::Parse(header, &error));
  if (!crx) {
    switch (error) {
      case CrxFile::kWrongMagic:
        ReportFailure(
            CRX_MAGIC_NUMBER_INVALID,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PACKAGE_ERROR_CODE,
                ASCIIToUTF16("CRX_MAGIC_NUMBER_INVALID")));
        break;
      case CrxFile::kInvalidVersion:
        // Bad version numer
        ReportFailure(
            CRX_VERSION_NUMBER_INVALID,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PACKAGE_ERROR_CODE,
                ASCIIToUTF16("CRX_VERSION_NUMBER_INVALID")));
        break;
      case CrxFile::kInvalidKeyTooLarge:
      case CrxFile::kInvalidSignatureTooLarge:
        // Excessively large key or signature
        ReportFailure(
            CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PACKAGE_ERROR_CODE,
                ASCIIToUTF16("CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE")));
        break;
      case CrxFile::kInvalidKeyTooSmall:
        // Key length is zero
        ReportFailure(
            CRX_ZERO_KEY_LENGTH,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PACKAGE_ERROR_CODE,
                ASCIIToUTF16("CRX_ZERO_KEY_LENGTH")));
        break;
      case CrxFile::kInvalidSignatureTooSmall:
        // Signature length is zero
        ReportFailure(
            CRX_ZERO_SIGNATURE_LENGTH,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PACKAGE_ERROR_CODE,
                ASCIIToUTF16("CRX_ZERO_SIGNATURE_LENGTH")));
        break;
    }
    return false;
  }

  std::vector<uint8> key;
  key.resize(header.key_size);
  len = fread(&key.front(), sizeof(uint8), header.key_size, file.get());
  if (len < header.key_size) {
    // Invalid public key
    ReportFailure(
        CRX_PUBLIC_KEY_INVALID,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_PUBLIC_KEY_INVALID")));
    return false;
  }

  std::vector<uint8> signature;
  signature.resize(header.signature_size);
  len = fread(&signature.front(), sizeof(uint8), header.signature_size,
      file.get());
  if (len < header.signature_size) {
    // Invalid signature
    ReportFailure(
        CRX_SIGNATURE_INVALID,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_INVALID")));
    return false;
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crx_file::kSignatureAlgorithm,
                           sizeof(crx_file::kSignatureAlgorithm),
                           &signature.front(),
                           signature.size(),
                           &key.front(),
                           key.size())) {
    // Signature verification initialization failed. This is most likely
    // caused by a public key in the wrong format (should encode algorithm).
    ReportFailure(
        CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED")));
    return false;
  }

  unsigned char buf[1 << 12];
  while ((len = fread(buf, 1, sizeof(buf), file.get())) > 0)
    verifier.VerifyUpdate(buf, len);

  if (!verifier.VerifyFinal()) {
    // Signature verification failed
    ReportFailure(
        CRX_SIGNATURE_VERIFICATION_FAILED,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_ERROR_CODE,
            ASCIIToUTF16("CRX_SIGNATURE_VERIFICATION_FAILED")));
    return false;
  }

  std::string public_key =
      std::string(reinterpret_cast<char*>(&key.front()), key.size());
  base::Base64Encode(public_key, &public_key_);

  extension_id_ = crx_file::id_util::GenerateId(public_key);

  return true;
}

void SandboxedUnpacker::ReportFailure(FailureReason reason,
                                      const base::string16& error) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.SandboxUnpackFailureReason",
                            reason, NUM_FAILURE_REASONS);
  UMA_HISTOGRAM_TIMES("Extensions.SandboxUnpackFailureTime",
                      base::TimeTicks::Now() - unpack_start_time_);
  Cleanup();
  client_->OnUnpackFailure(error);
}

void SandboxedUnpacker::ReportSuccess(
    const base::DictionaryValue& original_manifest,
    const SkBitmap& install_icon) {
  UMA_HISTOGRAM_COUNTS("Extensions.SandboxUnpackSuccess", 1);

  RecordSuccessfulUnpackTimeHistograms(
      crx_path_, base::TimeTicks::Now() - unpack_start_time_);

  // Client takes ownership of temporary directory and extension.
  client_->OnUnpackSuccess(
      temp_dir_.Take(), extension_root_, &original_manifest, extension_.get(),
      install_icon);
  extension_ = NULL;
}

base::DictionaryValue* SandboxedUnpacker::RewriteManifestFile(
    const base::DictionaryValue& manifest) {
  // Add the public key extracted earlier to the parsed manifest and overwrite
  // the original manifest. We do this to ensure the manifest doesn't contain an
  // exploitable bug that could be used to compromise the browser.
  scoped_ptr<base::DictionaryValue> final_manifest(manifest.DeepCopy());
  final_manifest->SetString(manifest_keys::kPublicKey, public_key_);

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*final_manifest)) {
    // Error serializing manifest.json.
    ReportFailure(
        ERROR_SERIALIZING_MANIFEST_JSON,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("ERROR_SERIALIZING_MANIFEST_JSON")));
    return NULL;
  }

  base::FilePath manifest_path =
      extension_root_.Append(kManifestFilename);
  int size = base::checked_cast<int>(manifest_json.size());
  if (base::WriteFile(manifest_path, manifest_json.data(), size) != size) {
    // Error saving manifest.json.
    ReportFailure(
        ERROR_SAVING_MANIFEST_JSON,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("ERROR_SAVING_MANIFEST_JSON")));
    return NULL;
  }

  return final_manifest.release();
}

bool SandboxedUnpacker::RewriteImageFiles(SkBitmap* install_icon) {
  DecodedImages images;
  if (!ReadImagesFromFile(temp_dir_.path(), &images)) {
    // Couldn't read image data from disk.
    ReportFailure(
        COULD_NOT_READ_IMAGE_DATA_FROM_DISK,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_READ_IMAGE_DATA_FROM_DISK")));
    return false;
  }

  // Delete any images that may be used by the browser.  We're going to write
  // out our own versions of the parsed images, and we want to make sure the
  // originals are gone for good.
  std::set<base::FilePath> image_paths =
      extension_file_util::GetBrowserImagePaths(extension_.get());
  if (image_paths.size() != images.size()) {
    // Decoded images don't match what's in the manifest.
    ReportFailure(
        DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("DECODED_IMAGES_DO_NOT_MATCH_THE_MANIFEST")));
    return false;
  }

  for (std::set<base::FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    base::FilePath path = *it;
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Invalid path for browser image.
      ReportFailure(
          INVALID_PATH_FOR_BROWSER_IMAGE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_BROWSER_IMAGE")));
      return false;
    }
    if (!base::DeleteFile(extension_root_.Append(path), false)) {
      // Error removing old image file.
      ReportFailure(
          ERROR_REMOVING_OLD_IMAGE_FILE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_REMOVING_OLD_IMAGE_FILE")));
      return false;
    }
  }

  const std::string& install_icon_path =
      IconsInfo::GetIcons(extension_.get()).Get(
          extension_misc::EXTENSION_ICON_LARGE, ExtensionIconSet::MATCH_BIGGER);

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    if (BrowserThread::GetBlockingPool()->IsShutdownInProgress()) {
      // Abort package installation if shutdown was initiated, crbug.com/235525
      ReportFailure(
          ABORTED_DUE_TO_SHUTDOWN,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ABORTED_DUE_TO_SHUTDOWN")));
      return false;
    }

    const SkBitmap& image = images[i].a;
    base::FilePath path_suffix = images[i].b;
    if (path_suffix.MaybeAsASCII() == install_icon_path)
      *install_icon = image;

    if (path_suffix.IsAbsolute() || path_suffix.ReferencesParent()) {
      // Invalid path for bitmap image.
      ReportFailure(
          INVALID_PATH_FOR_BITMAP_IMAGE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_BITMAP_IMAGE")));
      return false;
    }
    base::FilePath path = extension_root_.Append(path_suffix);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data)) {
      // Error re-encoding theme image.
      ReportFailure(
          ERROR_RE_ENCODING_THEME_IMAGE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_RE_ENCODING_THEME_IMAGE")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    int size = base::checked_cast<int>(image_data.size());
    if (base::WriteFile(path, image_data_ptr, size) != size) {
      // Error saving theme image.
      ReportFailure(
          ERROR_SAVING_THEME_IMAGE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SAVING_THEME_IMAGE")));
      return false;
    }
  }

  return true;
}

bool SandboxedUnpacker::RewriteCatalogFiles() {
  base::DictionaryValue catalogs;
  if (!ReadMessageCatalogsFromFile(temp_dir_.path(), &catalogs)) {
    // Could not read catalog data from disk.
    ReportFailure(
        COULD_NOT_READ_CATALOG_DATA_FROM_DISK,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
            ASCIIToUTF16("COULD_NOT_READ_CATALOG_DATA_FROM_DISK")));
    return false;
  }

  // Write our parsed catalogs back to disk.
  for (base::DictionaryValue::Iterator it(catalogs);
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* catalog = NULL;
    if (!it.value().GetAsDictionary(&catalog)) {
      // Invalid catalog data.
      ReportFailure(
          INVALID_CATALOG_DATA,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_CATALOG_DATA")));
      return false;
    }

    base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(it.key());
    relative_path = relative_path.Append(kMessagesFilename);
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      // Invalid path for catalog.
      ReportFailure(
          INVALID_PATH_FOR_CATALOG,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("INVALID_PATH_FOR_CATALOG")));
      return false;
    }
    base::FilePath path = extension_root_.Append(relative_path);

    std::string catalog_json;
    JSONStringValueSerializer serializer(&catalog_json);
    serializer.set_pretty_print(true);
    if (!serializer.Serialize(*catalog)) {
      // Error serializing catalog.
      ReportFailure(
          ERROR_SERIALIZING_CATALOG,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SERIALIZING_CATALOG")));
      return false;
    }

    // Note: we're overwriting existing files that the utility process read,
    // so we can be sure the directory exists.
    int size = base::checked_cast<int>(catalog_json.size());
    if (base::WriteFile(path, catalog_json.c_str(), size) != size) {
      // Error saving catalog.
      ReportFailure(
          ERROR_SAVING_CATALOG,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              ASCIIToUTF16("ERROR_SAVING_CATALOG")));
      return false;
    }
  }

  return true;
}

void SandboxedUnpacker::Cleanup() {
  DCHECK(unpacker_io_task_runner_->RunsTasksOnCurrentThread());
  if (!temp_dir_.Delete()) {
    LOG(WARNING) << "Can not delete temp directory at "
                 << temp_dir_.path().value();
  }
}

}  // namespace extensions
