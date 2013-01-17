// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_split.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"

namespace {

// An arbitrary delay to coalesce multiple writes to the cache.
const int kValidationCacheCoalescingTimeMS = 6000;
const char kValidationCacheSequenceName[] = "NaClValidationCache";
const FilePath::CharType kValidationCacheFileName[] =
    FILE_PATH_LITERAL("nacl_validation_cache.bin");

const bool kValidationCacheEnabledByDefault = true;

enum ValidationCacheStatus {
  CACHE_MISS = 0,
  CACHE_HIT,
  CACHE_MAX
};

const FilePath::StringType NaClIrtName() {
  FilePath::StringType irt_name(FILE_PATH_LITERAL("nacl_irt_"));

#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  bool is64 = true;
#elif defined(OS_WIN)
  bool is64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
               base::win::OSInfo::WOW64_ENABLED);
#else
  bool is64 = false;
#endif
  if (is64)
    irt_name.append(FILE_PATH_LITERAL("x86_64"));
  else
    irt_name.append(FILE_PATH_LITERAL("x86_32"));

#elif defined(ARCH_CPU_ARMEL)
  // TODO(mcgrathr): Eventually we'll need to distinguish arm32 vs thumb2.
  // That may need to be based on the actual nexe rather than a static
  // choice, which would require substantial refactoring.
  irt_name.append(FILE_PATH_LITERAL("arm"));
#elif defined(ARCH_CPU_MIPSEL)
  irt_name.append(FILE_PATH_LITERAL("mips32"));
#else
#error Add support for your architecture to NaCl IRT file selection
#endif
  irt_name.append(FILE_PATH_LITERAL(".nexe"));
  return irt_name;
}

bool CheckEnvVar(const char* name, bool default_value) {
  bool result = default_value;
  const char* var = getenv(name);
  if (var && strlen(var) > 0) {
    result = var[0] != '0';
  }
  return result;
}

void ReadCache(const FilePath& filename, std::string* data) {
  if (!file_util::ReadFileToString(filename, data)) {
    // Zero-size data used as an in-band error code.
    data->clear();
  }
}

void WriteCache(const FilePath& filename, const Pickle* pickle) {
  file_util::WriteFile(filename, static_cast<const char*>(pickle->data()),
                       pickle->size());
}

void RemoveCache(const FilePath& filename, const base::Closure& callback) {
  file_util::Delete(filename, false);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   callback);
}

void LogCacheQuery(ValidationCacheStatus status) {
  UMA_HISTOGRAM_ENUMERATION("NaCl.ValidationCache.Query", status, CACHE_MAX);
}

void LogCacheSet(ValidationCacheStatus status) {
  // Bucket zero is reserved for future use.
  UMA_HISTOGRAM_ENUMERATION("NaCl.ValidationCache.Set", status, CACHE_MAX);
}

}  // namespace

NaClBrowser::NaClBrowser()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      irt_platform_file_(base::kInvalidPlatformFileValue),
      irt_filepath_(),
      irt_state_(NaClResourceUninitialized),
      debug_patterns_(),
      inverse_debug_patterns_(false),
      validation_cache_file_path_(),
      validation_cache_is_enabled_(
          CheckEnvVar("NACL_VALIDATION_CACHE",
                      kValidationCacheEnabledByDefault)),
      validation_cache_is_modified_(false),
      validation_cache_state_(NaClResourceUninitialized),
      ok_(true) {
  InitIrtFilePath();
  InitValidationCacheFilePath();
}

NaClBrowser::~NaClBrowser() {
  if (irt_platform_file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(irt_platform_file_);
}

void NaClBrowser::InitIrtFilePath() {
  // Allow the IRT library to be overridden via an environment
  // variable.  This allows the NaCl/Chromium integration bot to
  // specify a newly-built IRT rather than using a prebuilt one
  // downloaded via Chromium's DEPS file.  We use the same environment
  // variable that the standalone NaCl PPAPI plugin accepts.
  const char* irt_path_var = getenv("NACL_IRT_LIBRARY");
  if (irt_path_var != NULL) {
    FilePath::StringType path_string(
        irt_path_var, const_cast<const char*>(strchr(irt_path_var, '\0')));
    irt_filepath_ = FilePath(path_string);
  } else {
    FilePath plugin_dir;
    if (!PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &plugin_dir)) {
      DLOG(ERROR) << "Failed to locate the plugins directory, NaCl disabled.";
      MarkAsFailed();
      return;
    }
    irt_filepath_ = plugin_dir.Append(NaClIrtName());
  }
}

NaClBrowser* NaClBrowser::GetInstance() {
  return Singleton<NaClBrowser>::get();
}

bool NaClBrowser::IsReady() const {
  return (IsOk() &&
          irt_state_ == NaClResourceReady &&
          validation_cache_state_ == NaClResourceReady);
}

bool NaClBrowser::IsOk() const {
  return ok_;
}

base::PlatformFile NaClBrowser::IrtFile() const {
  CHECK_EQ(irt_state_, NaClResourceReady);
  CHECK_NE(irt_platform_file_, base::kInvalidPlatformFileValue);
  return irt_platform_file_;
}

void NaClBrowser::EnsureAllResourcesAvailable() {
  EnsureIrtAvailable();
  EnsureValidationCacheAvailable();
}

// Load the IRT async.
void NaClBrowser::EnsureIrtAvailable() {
  if (IsOk() && irt_state_ == NaClResourceUninitialized) {
    irt_state_ = NaClResourceRequested;
    // TODO(ncbray) use blocking pool.
    if (!base::FileUtilProxy::CreateOrOpen(
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::FILE),
            irt_filepath_,
            base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
            base::Bind(&NaClBrowser::OnIrtOpened,
                       weak_factory_.GetWeakPtr()))) {
      LOG(ERROR) << "Internal error, NaCl disabled.";
      MarkAsFailed();
    }
  }
}

void NaClBrowser::OnIrtOpened(base::PlatformFileError error_code,
                              base::PassPlatformFile file,
                              bool created) {
  DCHECK_EQ(irt_state_, NaClResourceRequested);
  DCHECK(!created);
  if (error_code == base::PLATFORM_FILE_OK) {
    irt_platform_file_ = file.ReleaseValue();
  } else {
    LOG(ERROR) << "Failed to open NaCl IRT file \""
               << irt_filepath_.LossyDisplayName()
               << "\": " << error_code;
    MarkAsFailed();
  }
  irt_state_ = NaClResourceReady;
  CheckWaiting();
}

void NaClBrowser::SetDebugPatterns(std::string debug_patterns) {
  if (!debug_patterns.empty() && debug_patterns[0] == '!') {
    inverse_debug_patterns_ = true;
    debug_patterns.erase(0, 1);
  }
  if (debug_patterns.empty()) {
    return;
  }
  std::vector<std::string> patterns;
  base::SplitString(debug_patterns, ',', &patterns);
  for (std::vector<std::string>::iterator iter = patterns.begin();
       iter != patterns.end(); ++iter) {
    URLPattern pattern;
    if (pattern.Parse(*iter) == URLPattern::PARSE_SUCCESS) {
      // If URL pattern has scheme equal to *, Parse method resets valid
      // schemes mask to http and https only, so we need to reset it after
      // Parse to include chrome-extension scheme that can be used by NaCl
      // manifest files.
      pattern.SetValidSchemes(URLPattern::SCHEME_ALL);
      debug_patterns_.push_back(pattern);
    }
  }
}

bool NaClBrowser::URLMatchesDebugPatterns(GURL manifest_url) {
  // Empty patterns are forbidden so we ignore them.
  if (debug_patterns_.empty()) {
    return true;
  }
  bool matches = false;
  for (std::vector<URLPattern>::iterator iter = debug_patterns_.begin();
       iter != debug_patterns_.end(); ++iter) {
    if (iter->MatchesURL(manifest_url)) {
      matches = true;
      break;
    }
  }
  if (inverse_debug_patterns_) {
    return !matches;
  } else {
    return matches;
  }
}

void NaClBrowser::FireGdbDebugStubPortOpened(int port) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(debug_stub_port_listener_, port));
}

bool NaClBrowser::HasGdbDebugStubPortListener() {
  return !debug_stub_port_listener_.is_null();
}

void NaClBrowser::SetGdbDebugStubPortListener(
    base::Callback<void(int)> listener) {
  debug_stub_port_listener_ = listener;
}

void NaClBrowser::ClearGdbDebugStubPortListener() {
  debug_stub_port_listener_.Reset();
}

void NaClBrowser::InitValidationCacheFilePath() {
  // Determine where the validation cache resides in the file system.  It
  // exists in Chrome's cache directory and is not tied to any specific
  // profile.
  // Start by finding the user data directory.
  FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    RunWithoutValidationCache();
    return;
  }
  // The cache directory may or may not be the user data directory.
  FilePath cache_file_path;
  chrome::GetUserCacheDirectory(user_data_dir, &cache_file_path);
  // Append the base file name to the cache directory.

  validation_cache_file_path_ =
      cache_file_path.Append(kValidationCacheFileName);
}

void NaClBrowser::EnsureValidationCacheAvailable() {
  if (IsOk() && validation_cache_state_ == NaClResourceUninitialized) {
    if (ValidationCacheIsEnabled()) {
      validation_cache_state_ = NaClResourceRequested;

      // Structure for carrying data between the callbacks.
      std::string* data = new std::string();
      // We can get away not giving this a sequence ID because this is the first
      // task and further file access will not occur until after we get a
      // response.
      if (!content::BrowserThread::PostBlockingPoolTaskAndReply(
              FROM_HERE,
              base::Bind(ReadCache, validation_cache_file_path_, data),
              base::Bind(&NaClBrowser::OnValidationCacheLoaded,
                         weak_factory_.GetWeakPtr(),
                         base::Owned(data)))) {
        RunWithoutValidationCache();
      }
    } else {
      RunWithoutValidationCache();
    }
  }
}

void NaClBrowser::OnValidationCacheLoaded(const std::string *data) {
  // Did the cache get cleared before the load completed?  If so, ignore the
  // incoming data.
  if (validation_cache_state_ == NaClResourceReady)
    return;

  if (data->size() == 0) {
    // No file found.
    validation_cache_.Reset();
  } else {
    Pickle pickle(data->data(), data->size());
    validation_cache_.Deserialize(&pickle);
  }
  validation_cache_state_ = NaClResourceReady;
  CheckWaiting();
}

void NaClBrowser::RunWithoutValidationCache() {
  // Be paranoid.
  validation_cache_.Reset();
  validation_cache_is_enabled_ = false;
  validation_cache_state_ = NaClResourceReady;
  CheckWaiting();
}

void NaClBrowser::CheckWaiting() {
  if (!IsOk() || IsReady()) {
    // Queue the waiting tasks into the message loop.  This helps avoid
    // re-entrancy problems that could occur if the closure was invoked
    // directly.  For example, this could result in use-after-free of the
    // process host.
    for (std::vector<base::Closure>::iterator iter = waiting_.begin();
         iter != waiting_.end(); ++iter) {
      MessageLoop::current()->PostTask(FROM_HERE, *iter);
    }
    waiting_.clear();
  }
}

void NaClBrowser::MarkAsFailed() {
  ok_ = false;
  CheckWaiting();
}

void NaClBrowser::WaitForResources(const base::Closure& reply) {
  waiting_.push_back(reply);
  EnsureAllResourcesAvailable();
  CheckWaiting();
}

const FilePath& NaClBrowser::GetIrtFilePath() {
  return irt_filepath_;
}

bool NaClBrowser::QueryKnownToValidate(const std::string& signature,
                                       bool off_the_record) {
  if (off_the_record) {
    // If we're off the record, don't reorder the main cache.
    return validation_cache_.QueryKnownToValidate(signature, false) ||
        off_the_record_validation_cache_.QueryKnownToValidate(signature, true);
  } else {
    bool result = validation_cache_.QueryKnownToValidate(signature, true);
    LogCacheQuery(result ? CACHE_HIT : CACHE_MISS);
    // Queries can modify the MRU order of the cache.
    MarkValidationCacheAsModified();
    return result;
  }
}

void NaClBrowser::SetKnownToValidate(const std::string& signature,
                                     bool off_the_record) {
  if (off_the_record) {
    off_the_record_validation_cache_.SetKnownToValidate(signature);
  } else {
    validation_cache_.SetKnownToValidate(signature);
    // The number of sets should be equal to the number of cache misses, minus
    // validation failures and successful validations where stubout occurs.
    LogCacheSet(CACHE_HIT);
    MarkValidationCacheAsModified();
  }
}

void NaClBrowser::ClearValidationCache(const base::Closure& callback) {
  // Note: this method may be called before EnsureValidationCacheAvailable has
  // been invoked.  In other words, this method may be called before any NaCl
  // processes have been created.  This method must succeed and invoke the
  // callback in such a case.  If it does not invoke the callback, Chrome's UI
  // will hang in that case.
  validation_cache_.Reset();
  off_the_record_validation_cache_.Reset();

  if (validation_cache_file_path_.empty()) {
    // Can't figure out what file to remove, but don't drop the callback.
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     callback);
  } else {
    // Delegate the removal of the cache from the filesystem to another thread
    // to avoid blocking the IO thread.
    // This task is dispatched immediately, not delayed and coalesced, because
    // the user interface for cache clearing is likely waiting for the callback.
    // In addition, we need to make sure the cache is actually cleared before
    // invoking the callback to meet the implicit guarantees of the UI.
    content::BrowserThread::PostBlockingPoolSequencedTask(
        kValidationCacheSequenceName,
        FROM_HERE,
        base::Bind(RemoveCache, validation_cache_file_path_, callback));
  }

  // Make sure any delayed tasks to persist the cache to the filesystem are
  // squelched.
  validation_cache_is_modified_ = false;

  // If the cache is cleared before it is loaded from the filesystem, act as if
  // we just loaded an empty cache.
  if (validation_cache_state_ != NaClResourceReady) {
    validation_cache_state_ = NaClResourceReady;
    CheckWaiting();
  }
}

void NaClBrowser::MarkValidationCacheAsModified() {
  if (!validation_cache_is_modified_) {
    // Wait before persisting to disk.  This can coalesce multiple cache
    // modifications info a single disk write.
    MessageLoop::current()->PostDelayedTask(
         FROM_HERE,
         base::Bind(&NaClBrowser::PersistValidationCache,
                    weak_factory_.GetWeakPtr()),
         base::TimeDelta::FromMilliseconds(kValidationCacheCoalescingTimeMS));
    validation_cache_is_modified_ = true;
  }
}

void NaClBrowser::PersistValidationCache() {
  // validation_cache_is_modified_ may be false if the cache was cleared while
  // this delayed task was pending.
  // validation_cache_file_path_ may be empty if something went wrong during
  // initialization.
  if (validation_cache_is_modified_ && !validation_cache_file_path_.empty()) {
    Pickle* pickle = new Pickle();
    validation_cache_.Serialize(pickle);

    // Pass the serialized data to another thread to write to disk.  File IO is
    // not allowed on the IO thread (which is the thread this method runs on)
    // because it can degrade the responsiveness of the browser.
    // The task is sequenced so that multiple writes happen in order.
    content::BrowserThread::PostBlockingPoolSequencedTask(
        kValidationCacheSequenceName,
        FROM_HERE,
        base::Bind(WriteCache, validation_cache_file_path_,
                   base::Owned(pickle)));
  }
  validation_cache_is_modified_ = false;
}
