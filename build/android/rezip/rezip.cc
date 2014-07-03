// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// rezip is a tool which is used to modify zip files. It reads in a
// zip file and outputs a new zip file after applying various
// transforms. The tool is used in the Android Chromium build process
// to modify an APK file (which are zip files). The main application
// of this is to modify the APK so that the shared library is no
// longer compressed. Ironically, this saves both transmission and
// device drive space. It saves transmission space because
// uncompressed libraries make much smaller deltas with previous
// versions. It saves device drive space because it is no longer
// necessary to have both a compressed and uncompressed shared library
// on the device. To achieve this the uncompressed library is opened
// directly from within the APK using the "crazy" linker.

#include <assert.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>

#include "third_party/zlib/contrib/minizip/unzip.h"
#include "third_party/zlib/contrib/minizip/zip.h"

const int kMaxFilenameInZip = 256;
const int kMaxExtraFieldInZip = 8192;
const int kBufferSize = 4096;
// Note do not use sysconf(_SC_PAGESIZE) here as that will give you the
// page size of the host, this should be the page size of the target.
const int kPageSizeOnDevice = 4096;

// This is done to avoid having to make a dependency on all of base.
class LogStream {
 public:
  ~LogStream() {
    stream_.flush();
    std::cerr << stream_.str() << std::endl;
  }
  std::ostream& stream() {
    return stream_;
  }
 private:
  std::ostringstream stream_;
};

#define LOG(tag) (LogStream().stream() << #tag << ":")

// Copy the data from the currently opened file in the zipfile we are unzipping
// into the currently opened file of the zipfile we are zipping.
static bool CopySubfile(unzFile in_file,
                        zipFile out_file,
                        const char* in_zip_filename,
                        const char* out_zip_filename,
                        const char* in_filename,
                        const char* out_filename) {
  char buf[kBufferSize];

  int bytes = 0;
  while (true) {
    bytes = unzReadCurrentFile(in_file, buf, sizeof(buf));
    if (bytes < 0) {
      LOG(ERROR) << "failed to read from " << in_filename << " in zipfile "
                 << in_zip_filename;
      return false;
    }

    if (bytes == 0) {
      break;
    }

    if (ZIP_OK != zipWriteInFileInZip(out_file, buf, bytes)) {
      LOG(ERROR) << "failed to write from " << out_filename << " in zipfile "
                 << out_zip_filename;
      return false;
    }
  }

  return true;
}

static zip_fileinfo BuildOutInfo(const unz_file_info& in_info) {
  zip_fileinfo out_info;
  out_info.tmz_date.tm_sec = in_info.tmu_date.tm_sec;
  out_info.tmz_date.tm_min = in_info.tmu_date.tm_min;
  out_info.tmz_date.tm_hour = in_info.tmu_date.tm_hour;
  out_info.tmz_date.tm_mday = in_info.tmu_date.tm_mday;
  out_info.tmz_date.tm_mon = in_info.tmu_date.tm_mon;
  out_info.tmz_date.tm_year = in_info.tmu_date.tm_year;

  out_info.dosDate = in_info.dosDate;
  out_info.internal_fa = in_info.internal_fa;
  out_info.external_fa = in_info.external_fa;
  return out_info;
}

// RAII pattern for closing the unzip file.
class ScopedUnzip {
 public:
  ScopedUnzip(const char* z_filename)
      : z_file_(NULL), z_filename_(z_filename) {}

  unzFile OpenOrDie() {
    z_file_ = unzOpen(z_filename_);
    if (z_file_ == NULL) {
      LOG(ERROR) << "failed to open zipfile " << z_filename_;
      exit(1);
    }
    return z_file_;
  }

  ~ScopedUnzip() {
    if (z_file_ != NULL && unzClose(z_file_) != UNZ_OK) {
      LOG(ERROR) << "failed to close input zipfile " << z_filename_;
      exit(1);
    }
  }

 private:
  const char* z_filename_;
  unzFile z_file_;
};

// RAII pattern for closing the out zip file.
class ScopedZip {
 public:
  ScopedZip(const char* z_filename)
      : z_file_(NULL), z_filename_(z_filename) {}

  zipFile OpenOrDie() {
    z_file_ = zipOpen(z_filename_, APPEND_STATUS_CREATE);
    if (z_file_ == NULL) {
      LOG(ERROR) << "failed to open zipfile " << z_filename_;
      exit(1);
    }
    return z_file_;
  }

  ~ScopedZip() {
    if (z_file_ != NULL && zipClose(z_file_, NULL) != ZIP_OK) {
      LOG(ERROR) << "failed to close output zipfile" << z_filename_;
      exit(1);
    }
  }

 private:
  const char* z_filename_;
  zipFile z_file_;
};

typedef std::string (*RenameFun)(const char* in_filename);
typedef int (*AlignFun)(const char* in_filename,
                        unzFile in_file,
                        char* extra_buffer,
                        int size);
typedef bool (*InflatePredicateFun)(const char* filename);

static bool IsPrefixLibraryFilename(const char* filename,
                                    const char* base_prefix) {
  // We are basically matching "lib/[^/]*/<base_prefix>lib.*[.]so".
  // However, we don't have C++11 regex, so we just handroll the test.
  // Also we exclude "libchromium_android_linker.so" as a match.
  const std::string filename_str = filename;
  const std::string prefix = "lib/";
  const std::string suffix = ".so";

  if (filename_str.length() < suffix.length() + prefix.length()) {
    // too short
    return false;
  }

  if (filename_str.compare(0, prefix.size(), prefix) != 0) {
    // does not start with "lib/"
    return false;
  }

  if (filename_str.compare(filename_str.length() - suffix.length(),
                           suffix.length(),
                           suffix) != 0) {
    // does not end with ".so"
    return false;
  }

  const size_t last_slash = filename_str.find_last_of('/');
  if (last_slash < prefix.length()) {
    // Only one slash
    return false;
  }

  const size_t second_slash = filename_str.find_first_of('/', prefix.length());
  if (second_slash != last_slash) {
    // filename_str contains more than two slashes.
    return false;
  }

  const std::string libprefix = std::string(base_prefix) + "lib";
  if (filename_str.compare(last_slash + 1, libprefix.length(), libprefix) !=
      0) {
    // basename piece does not start with <base_prefix>"lib"
    return false;
  }

  const std::string linker = "libchromium_android_linker.so";
  if (last_slash + 1 + linker.length() == filename_str.length() &&
      filename_str.compare(last_slash + 1, linker.length(), linker) == 0) {
    // Do not match the linker.
    return false;
  }
  return true;
}

static bool IsLibraryFilename(const char* filename) {
  return IsPrefixLibraryFilename(filename, "");
}

static bool IsCrazyLibraryFilename(const char* filename) {
  return IsPrefixLibraryFilename(filename, "crazy.");
}

static std::string RenameLibraryForCrazyLinker(const char* in_filename) {
  if (!IsLibraryFilename(in_filename)) {
    // Don't rename
    return in_filename;
  }

  std::string filename_str = in_filename;
  size_t last_slash = filename_str.find_last_of('/');
  if (last_slash == std::string::npos ||
      last_slash == filename_str.length() - 1) {
    return in_filename;
  }

  // We rename the library, so that the Android Package Manager
  // no longer extracts the library.
  const std::string basename_prefix = "crazy.";
  return filename_str.substr(0, last_slash + 1) + basename_prefix +
         filename_str.substr(last_slash + 1);
}

// For any file which matches the crazy library pattern "lib/../crazy.lib*.so"
// add sufficient padding to the header that the start of the file will be
// page aligned on the target device.
static int PageAlignCrazyLibrary(const char* in_filename,
                                 unzFile in_file,
                                 char* extra_buffer,
                                 int extra_size) {
  if (!IsCrazyLibraryFilename(in_filename)) {
    return extra_size;
  }
  const ZPOS64_T pos = unzGetCurrentFileZStreamPos64(in_file);
  const int padding = kPageSizeOnDevice - (pos % kPageSizeOnDevice);
  if (padding == kPageSizeOnDevice) {
    return extra_size;
  }

  assert(extra_size < kMaxExtraFieldInZip - padding);
  memset(extra_buffer + extra_size, 0, padding);
  return extra_size + padding;
}

// As only the read side API provides offsets, we check that we added the
// correct amount of padding by reading the zip file we just generated.
// Also enforce that only one library is in the APK.
static bool CheckPageAlignAndOnlyOneLibrary(const char* out_zip_filename) {
  ScopedUnzip scoped_unzip(out_zip_filename);
  unzFile in_file = scoped_unzip.OpenOrDie();

  int err = 0;
  int count = 0;
  bool checked = false;
  while (true) {
    char in_filename[kMaxFilenameInZip + 1];
    // Get info and extra field for current file.
    unz_file_info in_info;
    err = unzGetCurrentFileInfo(in_file,
                                &in_info,
                                in_filename,
                                sizeof(in_filename) - 1,
                                NULL,
                                0,
                                NULL,
                                0);
    if (err != UNZ_OK) {
      LOG(ERROR) << "failed to get filename" << out_zip_filename;
      return false;
    }
    assert(in_info.size_filename <= kMaxFilenameInZip);
    in_filename[in_info.size_filename] = '\0';

    if (IsCrazyLibraryFilename(in_filename)) {
      count++;
      if (count > 1) {
        LOG(ERROR)
            << "Found more than one library in " << out_zip_filename << "\n"
            << "Multiple libraries are not supported for APKs that use "
            << "'load_library_from_zip_file'.\n"
            << "See crbug/388223.\n"
            << "Note, check that your build is clean.\n"
            << "An unclean build can incorrectly incorporate old "
            << "libraries in the APK.";
        return false;
      }
      err = unzOpenCurrentFile(in_file);
      if (err != UNZ_OK) {
        LOG(ERROR) << "failed to open subfile" << out_zip_filename << " "
                   << in_filename;
        return false;
      }

      const ZPOS64_T pos = unzGetCurrentFileZStreamPos64(in_file);
      const int alignment = pos % kPageSizeOnDevice;
      checked = (alignment == 0);
      if (!checked) {
        LOG(ERROR) << "Failed to page align library " << in_filename
                   << ", position " << pos << " alignment " << alignment;
      }

      err = unzCloseCurrentFile(in_file);
      if (err != UNZ_OK) {
        LOG(ERROR) << "failed to close subfile" << out_zip_filename << " "
                   << in_filename;
        return false;
      }
    }

    const int next = unzGoToNextFile(in_file);
    if (next == UNZ_END_OF_LIST_OF_FILE) {
      break;
    }
    if (next != UNZ_OK) {
      LOG(ERROR) << "failed to go to next file" << out_zip_filename;
      return false;
    }
  }
  return checked;
}

// Copy files from one archive to another applying alignment, rename and
// inflate transformations if given.
static bool Rezip(const char* in_zip_filename,
                  const char* out_zip_filename,
                  AlignFun align_fun,
                  RenameFun rename_fun,
                  InflatePredicateFun inflate_predicate_fun) {
  ScopedUnzip scoped_unzip(in_zip_filename);
  unzFile in_file = scoped_unzip.OpenOrDie();

  ScopedZip scoped_zip(out_zip_filename);
  zipFile out_file = scoped_zip.OpenOrDie();
  if (unzGoToFirstFile(in_file) != UNZ_OK) {
    LOG(ERROR) << "failed to go to first file in " << in_zip_filename;
    return false;
  }

  int err = 0;
  while (true) {
    char in_filename[kMaxFilenameInZip + 1];
    // Get info and extra field for current file.
    char extra_buffer[kMaxExtraFieldInZip];
    unz_file_info in_info;
    err = unzGetCurrentFileInfo(in_file,
                                &in_info,
                                in_filename,
                                sizeof(in_filename) - 1,
                                &extra_buffer,
                                sizeof(extra_buffer),
                                NULL,
                                0);
    if (err != UNZ_OK) {
      LOG(ERROR) << "failed to get filename " << in_zip_filename;
      return false;
    }
    assert(in_info.size_filename <= kMaxFilenameInZip);
    in_filename[in_info.size_filename] = '\0';

    std::string out_filename = in_filename;
    if (rename_fun != NULL) {
      out_filename = rename_fun(in_filename);
    }

    bool inflate = false;
    if (inflate_predicate_fun != NULL) {
      inflate = inflate_predicate_fun(in_filename);
    }

    // Open the current file.
    int method = 0;
    int level = 0;
    int raw = !inflate;
    err = unzOpenCurrentFile2(in_file, &method, &level, raw);
    if (inflate) {
      method = Z_NO_COMPRESSION;
      level = 0;
    }

    if (err != UNZ_OK) {
      LOG(ERROR) << "failed to open subfile " << in_zip_filename << " "
                 << in_filename;
      return false;
    }

    // Get the extra field from the local header.
    char local_extra_buffer[kMaxExtraFieldInZip];
    int local_extra_size = unzGetLocalExtrafield(
        in_file, &local_extra_buffer, sizeof(local_extra_buffer));

    if (align_fun != NULL) {
      local_extra_size =
          align_fun(in_filename, in_file, local_extra_buffer, local_extra_size);
    }

    const char* local_extra = local_extra_size > 0 ? local_extra_buffer : NULL;
    const char* extra = in_info.size_file_extra > 0 ? extra_buffer : NULL;

    // Build the output info structure from the input info structure.
    const zip_fileinfo out_info = BuildOutInfo(in_info);

    const int ret = zipOpenNewFileInZip4(out_file,
                                         out_filename.c_str(),
                                         &out_info,
                                         local_extra,
                                         local_extra_size,
                                         extra,
                                         in_info.size_file_extra,
                                         /* comment */ NULL,
                                         method,
                                         level,
                                         /* raw */ 1,
                                         /* windowBits */ 0,
                                         /* memLevel */ 0,
                                         /* strategy */ 0,
                                         /* password */ NULL,
                                         /* crcForCrypting */ 0,
                                         in_info.version,
                                         /* flagBase */ 0);

    if (ZIP_OK != ret) {
      LOG(ERROR) << "failed to open subfile " << out_zip_filename << " "
                 << out_filename;
      return false;
    }

    if (!CopySubfile(in_file,
                     out_file,
                     in_zip_filename,
                     out_zip_filename,
                     in_filename,
                     out_filename.c_str())) {
      return false;
    }

    if (ZIP_OK != zipCloseFileInZipRaw(
                      out_file, in_info.uncompressed_size, in_info.crc)) {
      LOG(ERROR) << "failed to close subfile " << out_zip_filename << " "
                 << out_filename;
      return false;
    }

    err = unzCloseCurrentFile(in_file);
    if (err != UNZ_OK) {
      LOG(ERROR) << "failed to close subfile " << in_zip_filename << " "
                 << in_filename;
      return false;
    }
    const int next = unzGoToNextFile(in_file);
    if (next == UNZ_END_OF_LIST_OF_FILE) {
      break;
    }
    if (next != UNZ_OK) {
      LOG(ERROR) << "failed to go to next file" << in_zip_filename;
      return false;
    }
  }

  return true;
}

int main(int argc, const char* argv[]) {
  if (argc != 4) {
    LOG(ERROR) << "Usage: <action> <in_zipfile> <out_zipfile>";
    LOG(ERROR) << " <action> is 'inflatealign', 'dropdescriptors' or 'rename'";
    LOG(ERROR) << " 'inflatealign'";
    LOG(ERROR) << "   inflate and page aligns files of the form "
        "lib/*/crazy.lib*.so";
    LOG(ERROR) << " 'dropdescriptors':";
    LOG(ERROR) << "   remove zip data descriptors from the zip file";
    LOG(ERROR) << " 'rename':";
    LOG(ERROR) << "   renames files of the form lib/*/lib*.so to "
        "lib/*/crazy.lib*.so. Note libchromium_android_linker.so is "
        "not renamed as the crazy linker can not load itself.";
    exit(1);
  }

  const char* action = argv[1];
  const char* in_zip_filename = argv[2];
  const char* out_zip_filename = argv[3];

  InflatePredicateFun inflate_predicate_fun = NULL;
  AlignFun align_fun = NULL;
  RenameFun rename_fun = NULL;
  bool check_page_align = false;
  if (strcmp("inflatealign", action) == 0) {
    inflate_predicate_fun = &IsCrazyLibraryFilename;
    align_fun = &PageAlignCrazyLibrary;
    check_page_align = true;
  } else if (strcmp("rename", action) == 0) {
    rename_fun = &RenameLibraryForCrazyLinker;
  } else if (strcmp("dropdescriptors", action) == 0) {
    // Minizip does not know about data descriptors, so the default
    // copying action will drop the descriptors. This should be fine
    // as data descriptors are redundant information.
    // Note we need to explicitly drop the descriptors before trying to
    // do alignment otherwise we will miscalculate the position because
    // we don't know about the data descriptors.
  } else {
    LOG(ERROR) << "Usage: <action> should be 'inflatealign', "
                  "'dropdescriptors' or 'rename'";
    exit(1);
  }

  if (!Rezip(in_zip_filename,
             out_zip_filename,
             align_fun,
             rename_fun,
             inflate_predicate_fun)) {
    exit(1);
  }
  if (check_page_align && !CheckPageAlignAndOnlyOneLibrary(out_zip_filename)) {
    exit(1);
  }
  return 0;
}
