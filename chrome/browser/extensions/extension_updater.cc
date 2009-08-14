// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_updater.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/rand_util.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "libxml/tree.h"

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using prefs::kExtensionBlacklistUpdateVersion;
using prefs::kLastExtensionsUpdateCheck;
using prefs::kNextExtensionsUpdateCheck;

const char* ExtensionUpdater::kExpectedGupdateProtocol = "2.0";
const char* ExtensionUpdater::kExpectedGupdateXmlns =
    "http://www.google.com/update2/response";

// NOTE: HTTPS is used here to ensure the response from omaha can be trusted.
// The response contains a url for fetching the blacklist and a hash value
// for validation.
const char* ExtensionUpdater::kBlacklistUpdateUrl =
    "https://clients2.google.com/service/update2/crx";

// Update AppID for extesnion blacklist.
const char* ExtensionUpdater::kBlacklistAppID = "com.google.crx.blacklist";

// Wait at least 5 minutes after browser startup before we do any checks. If you
// change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60 * 5;

// For sanity checking on update frequency - enforced in release mode only.
static const int kMinUpdateFrequencySeconds = 30;
static const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// A utility class to do file handling on the file I/O thread.
class ExtensionUpdaterFileHandler
    : public base::RefCountedThreadSafe<ExtensionUpdaterFileHandler> {
 public:
  ExtensionUpdaterFileHandler(MessageLoop* updater_loop,
                              MessageLoop* file_io_loop)
      : updater_loop_(updater_loop), file_io_loop_(file_io_loop) {}

  // Writes crx file data into a tempfile, and calls back the updater.
  void WriteTempFile(const std::string& extension_id, const std::string& data,
                     scoped_refptr<ExtensionUpdater> updater) {
    // Make sure we're running in the right thread.
    DCHECK(MessageLoop::current() == file_io_loop_);

    FilePath path;
    if (!file_util::CreateTemporaryFileName(&path)) {
      LOG(WARNING) << "Failed to create temporary file path";
      return;
    }
    if (file_util::WriteFile(path, data.c_str(), data.length()) !=
        static_cast<int>(data.length())) {
      // TODO(asargent) - It would be nice to back off updating alltogether if
      // the disk is full. (http://crbug.com/12763).
      LOG(ERROR) << "Failed to write temporary file";
      file_util::Delete(path, false);
      return;
    }

    // The ExtensionUpdater is now responsible for cleaning up the temp file
    // from disk.
    updater_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        updater.get(), &ExtensionUpdater::OnCRXFileWritten, extension_id,
        path));
  }

  void DeleteFile(const FilePath& path) {
    DCHECK(MessageLoop::current() == file_io_loop_);
    if (!file_util::Delete(path, false)) {
      LOG(WARNING) << "Failed to delete temp file " << path.value();
    }
  }

 private:
  // The MessageLoop we use to call back the ExtensionUpdater.
  MessageLoop* updater_loop_;

  // The MessageLoop we should be operating on for file operations.
  MessageLoop* file_io_loop_;
};


ExtensionUpdater::ExtensionUpdater(ExtensionUpdateService* service,
                                   PrefService* prefs,
                                   int frequency_seconds,
                                   MessageLoop* file_io_loop)
    : service_(service), frequency_seconds_(frequency_seconds),
      file_io_loop_(file_io_loop), prefs_(prefs),
      file_handler_(new ExtensionUpdaterFileHandler(MessageLoop::current(),
                                                    file_io_loop_)) {
  Init();
}

void ExtensionUpdater::Init() {
  // Unless we're in a unit test, expect that the file_io_loop_ is on the
  // browser file thread.
  if (g_browser_process->file_thread() != NULL) {
    DCHECK(file_io_loop_ == g_browser_process->file_thread()->message_loop());
  }

  DCHECK_GE(frequency_seconds_, 5);
  DCHECK(frequency_seconds_ <= kMaxUpdateFrequencySeconds);
#ifdef NDEBUG
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds_ = std::max(frequency_seconds_, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds_ = std::min(frequency_seconds_, kMaxUpdateFrequencySeconds);
}

ExtensionUpdater::~ExtensionUpdater() {}

static void EnsureInt64PrefRegistered(PrefService* prefs,
                                      const wchar_t name[]) {
  if (!prefs->IsPrefRegistered(name))
    prefs->RegisterInt64Pref(name, 0);
}

static void EnsureBlacklistVersionPrefRegistered(PrefService* prefs) {
  if (!prefs->IsPrefRegistered(kExtensionBlacklistUpdateVersion))
    prefs->RegisterStringPref(kExtensionBlacklistUpdateVersion, L"0");
}

// The overall goal here is to balance keeping clients up to date while
// avoiding a thundering herd against update servers.
TimeDelta ExtensionUpdater::DetermineFirstCheckDelay() {
  // If someone's testing with a quick frequency, just allow it.
  if (frequency_seconds_ < kStartupWaitSeconds)
    return TimeDelta::FromSeconds(frequency_seconds_);

  // If we've never scheduled a check before, start at frequency_seconds_.
  if (!prefs_->HasPrefPath(kNextExtensionsUpdateCheck))
    return TimeDelta::FromSeconds(frequency_seconds_);

  // If it's been a long time since our last actual check, we want to do one
  // relatively soon.
  Time now = Time::Now();
  Time last = Time::FromInternalValue(prefs_->GetInt64(
      kLastExtensionsUpdateCheck));
  int days = (now - last).InDays();
  if (days >= 30) {
    // Wait 5-10 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds,
                                          kStartupWaitSeconds * 2));
  } else if (days >= 14) {
    // Wait 10-20 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds * 2,
                                          kStartupWaitSeconds * 4));
  } else if (days >= 3) {
    // Wait 20-40 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds * 4,
                                          kStartupWaitSeconds * 8));
  }

  // Read the persisted next check time, and use that if it isn't too soon.
  // Otherwise pick something random.
  Time saved_next = Time::FromInternalValue(prefs_->GetInt64(
      kNextExtensionsUpdateCheck));
  Time earliest = now + TimeDelta::FromSeconds(kStartupWaitSeconds);
  if (saved_next >= earliest) {
    return saved_next - now;
  } else {
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds,
                                          frequency_seconds_));
  }
}

void ExtensionUpdater::Start() {
  // Make sure our prefs are registered, then schedule the first check.
  EnsureInt64PrefRegistered(prefs_, kLastExtensionsUpdateCheck);
  EnsureInt64PrefRegistered(prefs_, kNextExtensionsUpdateCheck);
  EnsureBlacklistVersionPrefRegistered(prefs_);
  ScheduleNextCheck(DetermineFirstCheckDelay());
}

void ExtensionUpdater::Stop() {
  timer_.Stop();
  manifest_fetcher_.reset();
  extension_fetcher_.reset();
  manifests_pending_.clear();
  extensions_pending_.clear();
}

void ExtensionUpdater::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  if (source == manifest_fetcher_.get()) {
    OnManifestFetchComplete(url, status, response_code, data);
  } else if (source == extension_fetcher_.get()) {
    OnCRXFetchComplete(url, status, response_code, data);
  } else {
    NOTREACHED();
  }
}

void ExtensionUpdater::OnManifestFetchComplete(const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == URLRequestStatus::SUCCESS && response_code == 200) {
    ScopedVector<ParseResult> parsed;
    // TODO(asargent) - We should do the xml parsing in a sandboxed process.
    // (http://crbug.com/12677).
    if (Parse(data, &parsed.get())) {
      std::vector<int> updates = DetermineUpdates(parsed.get());
      for (size_t i = 0; i < updates.size(); i++) {
        ParseResult* update = parsed[updates[i]];
        FetchUpdatedExtension(update->extension_id, update->crx_url,
          update->package_hash, update->version->GetString());
      }
    }
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch manifst '" << url.possibly_invalid_spec() <<
        "' response code:" << response_code;
  }
  manifest_fetcher_.reset();

  // If we have any pending manifest requests, fire off the next one.
  if (!manifests_pending_.empty()) {
    GURL url = manifests_pending_.front();
    manifests_pending_.pop_front();
    StartUpdateCheck(url);
  }
}

void ExtensionUpdater::ProcessBlacklist(const std::string& data) {
  // Verify sha256 hash value.
  char sha256_hash_value[base::SHA256_LENGTH];
  base::SHA256HashString(data, sha256_hash_value, base::SHA256_LENGTH);
  std::string hash_in_hex = HexEncode(sha256_hash_value, base::SHA256_LENGTH);

  if (current_extension_fetch_.package_hash != hash_in_hex) {
    NOTREACHED() << "Fetched blacklist checksum is not as expected. "
      << "Expected: " << current_extension_fetch_.package_hash
      << " Actual: " << hash_in_hex;
    return;
  }
  std::vector<std::string> blacklist;
  SplitString(data, '\n', &blacklist);

  // Tell ExtensionService to update prefs.
  service_->UpdateExtensionBlacklist(blacklist);

  // Update the pref value for blacklist version
  prefs_->SetString(kExtensionBlacklistUpdateVersion,
    ASCIIToWide(current_extension_fetch_.version));
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionUpdater::OnCRXFetchComplete(const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const std::string& data) {
  if (url != current_extension_fetch_.url) {
    LOG(ERROR) << "Called with unexpected url:'" << url.spec()
               << "' expected:'" << current_extension_fetch_.url.spec() << "'";
    NOTREACHED();
  } else if (status.status() == URLRequestStatus::SUCCESS &&
             response_code == 200) {
    if (current_extension_fetch_.id == kBlacklistAppID) {
      ProcessBlacklist(data);
    } else {
      // Successfully fetched - now write crx to a file so we can have the
      // ExtensionsService install it.
      file_io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        file_handler_.get(), &ExtensionUpdaterFileHandler::WriteTempFile,
        current_extension_fetch_.id, data, this));
    }
  } else {
    // TODO(asargent) do things like exponential backoff, handling
    // 503 Service Unavailable / Retry-After headers, etc. here.
    // (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch extension '" <<
        url.possibly_invalid_spec() << "' response code:" << response_code;
  }
  extension_fetcher_.reset();
  current_extension_fetch_ = ExtensionFetch();

  // If there are any pending downloads left, start one.
  if (extensions_pending_.size() > 0) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url, next.package_hash, next.version);
  }
}

void ExtensionUpdater::OnCRXFileWritten(const std::string& id,
                                        const FilePath& path) {
  service_->UpdateExtension(id, path);
}

void ExtensionUpdater::OnExtensionInstallFinished(const FilePath& path,
                                                  Extension* extension) {
  // Have the file_handler_ delete the temp file on the file I/O thread.
  file_io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
    file_handler_.get(), &ExtensionUpdaterFileHandler::DeleteFile, path));
}


// Helper function for building up request parameters in update check urls. It
// appends information about one extension to a request parameter string. The
// format for request parameters in update checks is:
//
//   ?x=EXT1_INFO&x=EXT2_INFO
//
// where EXT1_INFO and EXT2_INFO are url-encoded strings of the form:
//
//   id=EXTENSION_ID&v=VERSION&uc
//
// So for two extensions like:
//   Extension 1- id:aaaa version:1.1
//   Extension 2- id:bbbb version:2.0
//
// the full update url would be:
//   http://somehost/path?x=id%3Daaaa%26v%3D1.1%26uc&x=id%3Dbbbb%26v%3D2.0%26uc
//
// (Note that '=' is %3D and '&' is %26 when urlencoded.)
//
// Again, this function would just append one extension's worth of data, e.g.
// "x=id%3Daaaa%26v%3D1.1%26uc"
void AppendExtensionInfo(std::string* str, const Extension& extension) {
    const Version* version = extension.version();
    DCHECK(version);
    std::vector<std::string> parts;

    // Push extension id, version, and uc (indicates an update check to Omaha).
    parts.push_back("id=" + extension.id());
    parts.push_back("v=" + version->GetString());
    parts.push_back("uc");

    str->append("x=" + EscapeQueryParamValue(JoinString(parts, '&')));
}

// Creates a blacklist update url.
GURL ExtensionUpdater::GetBlacklistUpdateUrl(const std::wstring& version) {
  std::string blklist_info = StringPrintf("id=%s&v=%s&uc", kBlacklistAppID,
    WideToASCII(version).c_str());
  return GURL(StringPrintf("%s?x=%s", kBlacklistUpdateUrl,
                           EscapeQueryParamValue(blklist_info).c_str()));
}

void ExtensionUpdater::ScheduleNextCheck(const TimeDelta& target_delay) {
  DCHECK(!timer_.IsRunning());
  DCHECK(target_delay >= TimeDelta::FromSeconds(1));

  // Add +/- 10% random jitter.
  double delay_ms = target_delay.InMillisecondsF();
  double jitter_factor = (RandDouble() * .2) - 0.1;
  delay_ms += delay_ms * jitter_factor;
  TimeDelta actual_delay = TimeDelta::FromMilliseconds(
      static_cast<int64>(delay_ms));

  // Save the time of next check.
  Time next = Time::Now() + actual_delay;
  prefs_->SetInt64(kNextExtensionsUpdateCheck, next.ToInternalValue());
  prefs_->ScheduleSavePersistentPrefs();

  timer_.Start(actual_delay, this, &ExtensionUpdater::TimerFired);
}

void ExtensionUpdater::TimerFired() {
  // Generate a set of update urls for loaded extensions.
  std::set<GURL> urls;

  // We always check blacklist update url
  urls.insert(GetBlacklistUpdateUrl(
    prefs_->GetString(kExtensionBlacklistUpdateVersion)));

  const ExtensionList* extensions = service_->extensions();
  for (ExtensionList::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    Extension* extension = (*iter);
    const GURL& update_url = extension->update_url();
    if (update_url.is_empty() || extension->id().empty()) {
      continue;
    }

    DCHECK(update_url.is_valid());
    DCHECK(!update_url.has_ref());

    // Append extension information to the url.
    std::string full_url_string = update_url.spec();
    full_url_string.append(update_url.has_query() ? "&" : "?");
    AppendExtensionInfo(&full_url_string, *extension);

    GURL full_url(full_url_string);
    if (!full_url.is_valid()) {
      LOG(ERROR) << "invalid url: " << full_url.possibly_invalid_spec();
      NOTREACHED();
    } else {
      urls.insert(full_url);
    }
  }
  // Now do an update check for each url we found.
  for (std::set<GURL>::iterator iter = urls.begin(); iter != urls.end();
       ++iter) {
    // StartUpdateCheck makes sure the url isn't already downloading or
    // scheduled, so we don't need to check before calling it.
    StartUpdateCheck(*iter);
  }
  // Save the last check time, and schedule the next check.
  int64 now = Time::Now().ToInternalValue();
  prefs_->SetInt64(kLastExtensionsUpdateCheck, now);
  ScheduleNextCheck(TimeDelta::FromSeconds(frequency_seconds_));
}

static void ManifestParseError(const char* details, ...) {
  va_list args;
  va_start(args, details);
  std::string message("Extension update manifest parse error: ");
  StringAppendV(&message, details, args);
  LOG(WARNING) << message;
}

// Checks whether a given node's name matches |expected_name| and
// |expected_namespace|.
static bool TagNameEquals(const xmlNode* node, const char* expected_name,
                          const xmlNs* expected_namespace) {
  if (node->ns != expected_namespace) {
    return false;
  }
  return 0 == strcmp(expected_name, reinterpret_cast<const char*>(node->name));
}

// Returns child nodes of |root| with name |name| in namespace |xml_namespace|.
static std::vector<xmlNode*> GetChildren(xmlNode* root, xmlNs* xml_namespace,
                                         const char* name) {
  std::vector<xmlNode*> result;
  for (xmlNode* child = root->children; child != NULL; child = child->next) {
    if (!TagNameEquals(child, name, xml_namespace)) {
      continue;
    }
    result.push_back(child);
  }
  return result;
}

// Returns the value of a named attribute, or the empty string.
static std::string GetAttribute(xmlNode* node, const char* attribute_name) {
  const xmlChar* name = reinterpret_cast<const xmlChar*>(attribute_name);
  for (xmlAttr* attr = node->properties; attr != NULL; attr = attr->next) {
    if (!xmlStrcmp(attr->name, name) && attr->children &&
        attr->children->content) {
      return std::string(reinterpret_cast<const char*>(
          attr->children->content));
    }
  }
  return std::string();
}

// This is used for the xml parser to report errors. This assumes the context
// is a pointer to a std::string where the error message should be appended.
static void XmlErrorFunc(void *context, const char *message, ...) {
  va_list args;
  va_start(args, message);
  std::string* error = static_cast<std::string*>(context);
  StringAppendV(error, message, args);
}

// Utility class for cleaning up xml parser state when leaving a scope.
class ScopedXmlParserCleanup {
 public:
  ScopedXmlParserCleanup() : document_(NULL) {}
  ~ScopedXmlParserCleanup() {
    if (document_)
      xmlFreeDoc(document_);
    xmlCleanupParser();
  }
  void set_document(xmlDocPtr document) {
    document_ = document;
  }

 private:
  xmlDocPtr document_;
};

// Returns a pointer to the xmlNs on |node| with the |expected_href|, or
// NULL if there isn't one with that href.
static xmlNs* GetNamespace(xmlNode* node, const char* expected_href) {
  const xmlChar* href = reinterpret_cast<const xmlChar*>(expected_href);
  for (xmlNs* ns = node->ns; ns != NULL; ns = ns->next) {
    if (ns->href && !xmlStrcmp(ns->href, href)) {
      return ns;
    }
  }
  return NULL;
}

// This is a separate sub-class so that we can have access to the private
// ParseResult struct, but avoid making the .h file include the xml api headers.
class ExtensionUpdater::ParseHelper {
 public:
  // Helper function for ExtensionUpdater::Parse that reads in values for a
  // single <app> tag. It returns a boolean indicating success or failure.
  static bool ParseSingleAppTag(xmlNode* app_node, xmlNs* xml_namespace,
                                ParseResult* result) {
    // Read the extension id.
    result->extension_id = GetAttribute(app_node, "appid");
    if (result->extension_id.length() == 0) {
      ManifestParseError("Missing appid on app node");
      return false;
    }

    // Get the updatecheck node.
    std::vector<xmlNode*> updates = GetChildren(app_node, xml_namespace,
                                                "updatecheck");
    if (updates.size() > 1) {
      ManifestParseError("Too many updatecheck tags on app (expecting only 1)");
      return false;
    }
    if (updates.size() == 0) {
      ManifestParseError("Missing updatecheck on app");
      return false;
    }
    xmlNode *updatecheck = updates[0];

    // Find the url to the crx file.
    result->crx_url = GURL(GetAttribute(updatecheck, "codebase"));
    if (!result->crx_url.is_valid()) {
      ManifestParseError("Invalid codebase url");
      return false;
    }

    // Get the version.
    std::string tmp = GetAttribute(updatecheck, "version");
    if (tmp.length() == 0) {
      ManifestParseError("Missing version for updatecheck");
      return false;
    }
    result->version.reset(Version::GetVersionFromString(tmp));
    if (!result->version.get()) {
      ManifestParseError("Invalid version");
      return false;
    }

    // Get the minimum browser version (not required).
    tmp = GetAttribute(updatecheck, "prodversionmin");
    if (tmp.length()) {
      result->browser_min_version.reset(Version::GetVersionFromString(tmp));
      if (!result->browser_min_version.get()) {
        ManifestParseError("Invalid prodversionmin");
        return false;
      }
    }

    // package_hash is optional. It is only required for blacklist. It is a
    // sha256 hash of the package in hex format.
    result->package_hash = GetAttribute(updatecheck, "hash");
    return true;
  }
};

bool ExtensionUpdater::Parse(const std::string& manifest_xml,
                                ParseResultList* results) {
  std::string xml_errors;
  ScopedXmlErrorFunc error_func(&xml_errors, &XmlErrorFunc);
  ScopedXmlParserCleanup xml_cleanup;

  xmlDocPtr document = xmlParseDoc(
      reinterpret_cast<const xmlChar*>(manifest_xml.c_str()));
  if (!document) {
    ManifestParseError(xml_errors.c_str());
    return false;
  }
  xml_cleanup.set_document(document);

  xmlNode *root = xmlDocGetRootElement(document);
  if (!root) {
    ManifestParseError("Missing root node");
    return false;
  }

  // Look for the required namespace declaration.
  xmlNs* gupdate_ns = GetNamespace(root, kExpectedGupdateXmlns);
  if (!gupdate_ns) {
    ManifestParseError("Missing or incorrect xmlns on gupdate tag");
    return false;
  }

  if (!TagNameEquals(root, "gupdate", gupdate_ns)) {
    ManifestParseError("Missing gupdate tag");
    return false;
  }

  // Check for the gupdate "protocol" attribute.
  if (GetAttribute(root, "protocol") != kExpectedGupdateProtocol) {
    ManifestParseError("Missing/incorrect protocol on gupdate tag "
        "(expected '%s')", kExpectedGupdateProtocol);
    return false;
  }

  // Parse each of the <app> tags.
  ScopedVector<ParseResult> tmp_results;
  std::vector<xmlNode*> apps = GetChildren(root, gupdate_ns, "app");
  for (unsigned int i = 0; i < apps.size(); i++) {
    ParseResult* current = new ParseResult();
    tmp_results.push_back(current);
    if (!ParseHelper::ParseSingleAppTag(apps[i], gupdate_ns, current)) {
      return false;
    }
  }
  results->insert(results->end(), tmp_results.begin(), tmp_results.end());
  tmp_results.get().clear();

  return true;
}

bool ExtensionUpdater::GetExistingVersion(const std::string& id,
                                          std::string* version) {
  if (id == kBlacklistAppID) {
    *version =
      WideToASCII(prefs_->GetString(kExtensionBlacklistUpdateVersion));
    return true;
  }
  Extension* extension = service_->GetExtensionById(id);
  if (!extension) {
    return false;
  }
  *version = extension->version()->GetString();
  return true;
}

std::vector<int> ExtensionUpdater::DetermineUpdates(
    const ParseResultList& possible_updates) {

  std::vector<int> result;

  // This will only get set if one of possible_updates specifies
  // browser_min_version.
  scoped_ptr<Version> browser_version;

  for (size_t i = 0; i < possible_updates.size(); i++) {
    ParseResult* update = possible_updates[i];

    std::string version;
    if (!GetExistingVersion(update->extension_id, &version)) {
      continue;
    }
    // If the update version is the same or older than what's already installed,
    // we don't want it.
    scoped_ptr<Version> existing_version(
      Version::GetVersionFromString(version));
    if (update->version.get()->CompareTo(*(existing_version.get())) <= 0) {
      continue;
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.get()) {
      // First determine the browser version if we haven't already.
      if (!browser_version.get()) {
        scoped_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfoForCurrentModule());
        if (version_info.get()) {
          browser_version.reset(Version::GetVersionFromString(
            version_info->product_version()));
        }
      }
      if (browser_version.get() &&
          update->browser_min_version->CompareTo(*browser_version.get()) > 0) {
        // TODO(asargent) - We may want this to show up in the extensions UI
        // eventually. (http://crbug.com/12547).
        LOG(WARNING) << "Updated version of extension " << update->extension_id
          << " available, but requires chrome version "
          << update->browser_min_version->GetString();
        continue;
      }
    }
    result.push_back(i);
  }
  return result;
}

void ExtensionUpdater::StartUpdateCheck(const GURL& url) {
  if (std::find(manifests_pending_.begin(), manifests_pending_.end(), url) !=
      manifests_pending_.end()) {
    return;  // already scheduled
  }

  if (manifest_fetcher_.get() != NULL) {
    if (manifest_fetcher_->url() != url) {
      manifests_pending_.push_back(url);
    }
  } else {
    manifest_fetcher_.reset(
        URLFetcher::Create(kManifestFetcherId, url, URLFetcher::GET, this));
    manifest_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    manifest_fetcher_->Start();
  }
}

void ExtensionUpdater::FetchUpdatedExtension(const std::string& id,
                                             const GURL& url,
                                             const std::string& hash,
                                             const std::string& version) {
  for (std::deque<ExtensionFetch>::const_iterator iter =
           extensions_pending_.begin();
       iter != extensions_pending_.end(); ++iter) {
    if (iter->id == id || iter->url == url) {
      return;  // already scheduled
    }
  }

  if (extension_fetcher_.get() != NULL) {
    if (extension_fetcher_->url() != url) {
      extensions_pending_.push_back(ExtensionFetch(id, url, hash, version));
    }
  } else {
    extension_fetcher_.reset(
        URLFetcher::Create(kExtensionFetcherId, url, URLFetcher::GET, this));
    extension_fetcher_->set_request_context(
        Profile::GetDefaultRequestContext());
    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url, hash, version);
  }
}
