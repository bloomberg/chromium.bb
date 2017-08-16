// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

namespace content {

namespace {

// Used internally only. These bit positions have no relationship to any
// underlying OS and can be changed to accommodate finer-grained permissions.
enum ChildProcessSecurityPermissions {
  READ_FILE_PERMISSION             = 1 << 0,
  WRITE_FILE_PERMISSION            = 1 << 1,
  CREATE_NEW_FILE_PERMISSION       = 1 << 2,
  CREATE_OVERWRITE_FILE_PERMISSION = 1 << 3,
  DELETE_FILE_PERMISSION           = 1 << 4,

  // Used by Media Galleries API
  COPY_INTO_FILE_PERMISSION        = 1 << 5,
};

// Used internally only. Bitmasks that are actually used by the Grant* and Can*
// methods. These contain one or more ChildProcessSecurityPermissions.
enum ChildProcessSecurityGrants {
  READ_FILE_GRANT              = READ_FILE_PERMISSION,
  WRITE_FILE_GRANT             = WRITE_FILE_PERMISSION,

  CREATE_NEW_FILE_GRANT        = CREATE_NEW_FILE_PERMISSION |
                                 COPY_INTO_FILE_PERMISSION,

  CREATE_READ_WRITE_FILE_GRANT = CREATE_NEW_FILE_PERMISSION |
                                 CREATE_OVERWRITE_FILE_PERMISSION |
                                 READ_FILE_PERMISSION |
                                 WRITE_FILE_PERMISSION |
                                 COPY_INTO_FILE_PERMISSION |
                                 DELETE_FILE_PERMISSION,

  COPY_INTO_FILE_GRANT         = COPY_INTO_FILE_PERMISSION,
  DELETE_FILE_GRANT            = DELETE_FILE_PERMISSION,
};

// https://crbug.com/646278 Valid blob URLs should contain canonically
// serialized origins.
bool IsMalformedBlobUrl(const GURL& url) {
  if (!url.SchemeIsBlob())
    return false;

  // If the part after blob: survives a roundtrip through url::Origin, then
  // it's a normal blob URL.
  std::string canonical_origin = url::Origin(url).Serialize();
  canonical_origin.append(1, '/');
  if (base::StartsWith(url.GetContent(), canonical_origin,
                       base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // blob:blobinternal:// is used by blink for stream URLs. This doesn't survive
  // url::Origin canonicalization -- blobinternal is a fake scheme -- but allow
  // it anyway. TODO(nick): Added speculatively, might be unnecessary.
  if (base::StartsWith(url.GetContent(), "blobinternal://",
                       base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // This is a malformed blob URL.
  return true;
}

}  // namespace

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicyImpl::SecurityState {
 public:
  SecurityState()
    : enabled_bindings_(0),
      can_read_raw_cookies_(false),
      can_send_midi_sysex_(false) { }

  ~SecurityState() {
    storage::IsolatedContext* isolated_context =
        storage::IsolatedContext::GetInstance();
    for (FileSystemMap::iterator iter = filesystem_permissions_.begin();
         iter != filesystem_permissions_.end();
         ++iter) {
      isolated_context->RemoveReference(iter->first);
    }
    UMA_HISTOGRAM_COUNTS("ChildProcessSecurityPolicy.PerChildFilePermissions",
                         file_permissions_.size());
  }

  // Grant permission to request URLs with the specified origin.
  void GrantOrigin(const url::Origin& origin) {
    origin_set_.insert(origin);
  }

  // Grant permission to request URLs with the specified scheme.
  void GrantScheme(const std::string& scheme) { scheme_policy_.insert(scheme); }

  // Grant certain permissions to a file.
  void GrantPermissionsForFile(const base::FilePath& file, int permissions) {
    base::FilePath stripped = file.StripTrailingSeparators();
    file_permissions_[stripped] |= permissions;
    UMA_HISTOGRAM_COUNTS("ChildProcessSecurityPolicy.FilePermissionPathLength",
                         stripped.value().size());
  }

  // Grant navigation to a file but not the file:// scheme in general.
  void GrantRequestOfSpecificFile(const base::FilePath &file) {
    request_file_set_.insert(file.StripTrailingSeparators());
  }

  // Revokes all permissions granted to a file.
  void RevokeAllPermissionsForFile(const base::FilePath& file) {
    base::FilePath stripped = file.StripTrailingSeparators();
    file_permissions_.erase(stripped);
    request_file_set_.erase(stripped);
  }

  // Grant certain permissions to a file.
  void GrantPermissionsForFileSystem(const std::string& filesystem_id,
                                     int permissions) {
    if (!base::ContainsKey(filesystem_permissions_, filesystem_id))
      storage::IsolatedContext::GetInstance()->AddReference(filesystem_id);
    filesystem_permissions_[filesystem_id] |= permissions;
  }

  bool HasPermissionsForFileSystem(const std::string& filesystem_id,
                                   int permissions) {
    FileSystemMap::const_iterator it =
        filesystem_permissions_.find(filesystem_id);
    if (it == filesystem_permissions_.end())
      return false;
    return (it->second & permissions) == permissions;
  }

#if defined(OS_ANDROID)
  // Determine if the certain permissions have been granted to a content URI.
  bool HasPermissionsForContentUri(const base::FilePath& file,
                                   int permissions) {
    DCHECK(!file.empty());
    DCHECK(file.IsContentUri());
    if (!permissions)
      return false;
    base::FilePath file_path = file.StripTrailingSeparators();
    FileMap::const_iterator it = file_permissions_.find(file_path);
    if (it != file_permissions_.end())
      return (it->second & permissions) == permissions;
    return false;
  }
#endif

  void GrantBindings(int bindings) {
    enabled_bindings_ |= bindings;
  }

  void GrantReadRawCookies() {
    can_read_raw_cookies_ = true;
  }

  void RevokeReadRawCookies() {
    can_read_raw_cookies_ = false;
  }

  void GrantPermissionForMidiSysEx() {
    can_send_midi_sysex_ = true;
  }

  bool CanCommitOrigin(const url::Origin& origin) {
    return base::ContainsKey(origin_set_, origin);
  }

  // Determine whether permission has been granted to commit |url|.
  bool CanCommitURL(const GURL& url) {
    DCHECK(!url.SchemeIsBlob() && !url.SchemeIsFileSystem())
        << "inner_url extraction should be done already.";
    // Having permission to a scheme implies permission to all of its URLs.
    SchemeSet::const_iterator scheme_judgment(
        scheme_policy_.find(url.scheme()));
    if (scheme_judgment != scheme_policy_.end())
      return true;

    // Otherwise, check for permission for specific origin.
    if (CanCommitOrigin(url::Origin(url)))
      return true;

    // file:// URLs are more granular.  The child may have been given
    // permission to a specific file but not the file:// scheme in general.
    if (url.SchemeIs(url::kFileScheme)) {
      base::FilePath path;
      if (net::FileURLToFilePath(url, &path))
        return base::ContainsKey(request_file_set_, path);
    }

    return false;  // Unmentioned schemes are disallowed.
  }

  // Determine if the certain permissions have been granted to a file.
  bool HasPermissionsForFile(const base::FilePath& file, int permissions) {
#if defined(OS_ANDROID)
    if (file.IsContentUri())
      return HasPermissionsForContentUri(file, permissions);
#endif
    if (!permissions || file.empty() || !file.IsAbsolute())
      return false;
    base::FilePath current_path = file.StripTrailingSeparators();
    base::FilePath last_path;
    int skip = 0;
    while (current_path != last_path) {
      base::FilePath base_name = current_path.BaseName();
      if (base_name.value() == base::FilePath::kParentDirectory) {
        ++skip;
      } else if (skip > 0) {
        if (base_name.value() != base::FilePath::kCurrentDirectory)
          --skip;
      } else {
        FileMap::const_iterator it = file_permissions_.find(current_path);
        if (it != file_permissions_.end())
          return (it->second & permissions) == permissions;
      }
      last_path = current_path;
      current_path = current_path.DirName();
    }

    return false;
  }

  bool CanAccessDataForOrigin(const GURL& site_url) {
    if (origin_lock_.is_empty())
      return true;
    return origin_lock_ == site_url;
  }

  void LockToOrigin(const GURL& gurl) {
    origin_lock_ = gurl;
  }

  ChildProcessSecurityPolicyImpl::CheckOriginLockResult CheckOriginLock(
      const GURL& gurl) {
    if (origin_lock_.is_empty())
      return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::NO_LOCK;

    if (origin_lock_ == gurl) {
      return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::
          HAS_EQUAL_LOCK;
    }

    return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::
        HAS_WRONG_LOCK;
  }

  bool has_web_ui_bindings() const {
    return enabled_bindings_ & BINDINGS_POLICY_WEB_UI;
  }

  bool can_read_raw_cookies() const {
    return can_read_raw_cookies_;
  }

  bool can_send_midi_sysex() const {
    return can_send_midi_sysex_;
  }

 private:
  typedef std::set<std::string> SchemeSet;
  typedef std::set<url::Origin> OriginSet;

  typedef int FilePermissionFlags;  // bit-set of base::File::Flags
  typedef std::map<base::FilePath, FilePermissionFlags> FileMap;
  typedef std::map<std::string, FilePermissionFlags> FileSystemMap;
  typedef std::set<base::FilePath> FileSet;

  // Maps URL schemes to whether permission has been granted, containment means
  // that the scheme has been granted, otherwise, it has never been granted.
  // There is no provision for revoking.
  SchemeSet scheme_policy_;

  // The set of URL origins to which the child process has been granted
  // permission.
  OriginSet origin_set_;

  // The set of files the child process is permited to upload to the web.
  FileMap file_permissions_;

  // The set of files the child process is permitted to load.
  FileSet request_file_set_;

  int enabled_bindings_;

  bool can_read_raw_cookies_;

  bool can_send_midi_sysex_;

  GURL origin_lock_;

  // The set of isolated filesystems the child process is permitted to access.
  FileSystemMap filesystem_permissions_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicyImpl::ChildProcessSecurityPolicyImpl() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(url::kHttpScheme);
  RegisterWebSafeScheme(url::kHttpsScheme);
  RegisterWebSafeScheme(url::kFtpScheme);
  RegisterWebSafeScheme(url::kDataScheme);
  RegisterWebSafeScheme("feed");

  // TODO(nick): https://crbug.com/651534 blob: and filesystem: schemes embed
  // other origins, so we should not treat them as web safe. Remove callers of
  // IsWebSafeScheme(), and then eliminate the next two lines.
  RegisterWebSafeScheme(url::kBlobScheme);
  RegisterWebSafeScheme(url::kFileSystemScheme);

  // We know about the following pseudo schemes and treat them specially.
  RegisterPseudoScheme(url::kAboutScheme);
  RegisterPseudoScheme(url::kJavaScriptScheme);
  RegisterPseudoScheme(kViewSourceScheme);
  RegisterPseudoScheme(url::kHttpSuboriginScheme);
  RegisterPseudoScheme(url::kHttpsSuboriginScheme);
}

ChildProcessSecurityPolicyImpl::~ChildProcessSecurityPolicyImpl() {
}

// static
ChildProcessSecurityPolicy* ChildProcessSecurityPolicy::GetInstance() {
  return ChildProcessSecurityPolicyImpl::GetInstance();
}

ChildProcessSecurityPolicyImpl* ChildProcessSecurityPolicyImpl::GetInstance() {
  return base::Singleton<ChildProcessSecurityPolicyImpl>::get();
}

void ChildProcessSecurityPolicyImpl::Add(int child_id) {
  base::AutoLock lock(lock_);
  AddChild(child_id);
}

void ChildProcessSecurityPolicyImpl::AddWorker(int child_id,
                                               int main_render_process_id) {
  base::AutoLock lock(lock_);
  AddChild(child_id);
  worker_map_[child_id] = main_render_process_id;
}

void ChildProcessSecurityPolicyImpl::Remove(int child_id) {
  base::AutoLock lock(lock_);
  security_state_.erase(child_id);
  worker_map_.erase(child_id);
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Add schemes at most once.";
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme))
      << "Web-safe implies not pseudo.";

  schemes_okay_to_request_in_any_process_.insert(scheme);
  schemes_okay_to_commit_in_any_process_.insert(scheme);
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeIsolatedScheme(
    const std::string& scheme,
    bool always_allow_in_origin_headers) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Add schemes at most once.";
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme))
      << "Web-safe implies not pseudo.";

  schemes_okay_to_request_in_any_process_.insert(scheme);
  if (always_allow_in_origin_headers)
    schemes_okay_to_appear_as_origin_headers_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return base::ContainsKey(schemes_okay_to_request_in_any_process_, scheme);
}

void ChildProcessSecurityPolicyImpl::RegisterPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0U, pseudo_schemes_.count(scheme)) << "Add schemes at most once.";
  DCHECK_EQ(0U, schemes_okay_to_request_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";
  DCHECK_EQ(0U, schemes_okay_to_commit_in_any_process_.count(scheme))
      << "Pseudo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return base::ContainsKey(pseudo_schemes_, scheme);
}

void ChildProcessSecurityPolicyImpl::GrantRequestURL(
    int child_id, const GURL& url) {

  if (!url.is_valid())
    return;  // Can't grant the capability to request invalid URLs.

  const std::string& scheme = url.scheme();

  if (IsWebSafeScheme(scheme))
    return;  // The scheme has already been whitelisted for every child process.

  if (IsPseudoScheme(scheme)) {
    return;  // Can't grant the capability to request pseudo schemes.
  }

  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    return;  // Don't grant blanket access to blob: or filesystem: schemes.
  }

  {
    base::AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return;

    // When the child process has been commanded to request this scheme,
    // we grant it the capability to request all URLs of that scheme.
    state->second->GrantScheme(scheme);
  }
}

void ChildProcessSecurityPolicyImpl::GrantRequestSpecificFileURL(
    int child_id,
    const GURL& url) {
  if (!url.SchemeIs(url::kFileScheme))
    return;

  {
    base::AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return;

    // When the child process has been commanded to request a file:// URL,
    // then we grant it the capability for that URL only.
    base::FilePath path;
    if (net::FileURLToFilePath(url, &path))
      state->second->GrantRequestOfSpecificFile(path);
  }
}

void ChildProcessSecurityPolicyImpl::GrantReadFile(int child_id,
                                                   const base::FilePath& file) {
  GrantPermissionsForFile(child_id, file, READ_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFile(
    int child_id, const base::FilePath& file) {
  GrantPermissionsForFile(child_id, file, CREATE_READ_WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCopyInto(int child_id,
                                                   const base::FilePath& dir) {
  GrantPermissionsForFile(child_id, dir, COPY_INTO_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantDeleteFrom(
    int child_id, const base::FilePath& dir) {
  GrantPermissionsForFile(child_id, dir, DELETE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionsForFile(file, permissions);
}

void ChildProcessSecurityPolicyImpl::RevokeAllPermissionsForFile(
    int child_id, const base::FilePath& file) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeAllPermissionsForFile(file);
}

void ChildProcessSecurityPolicyImpl::GrantReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, READ_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateFileForFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, CREATE_NEW_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(
      child_id, filesystem_id, CREATE_READ_WRITE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantCopyIntoFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, COPY_INTO_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantDeleteFromFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, DELETE_FILE_GRANT);
}

void ChildProcessSecurityPolicyImpl::GrantSendMidiSysExMessage(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionForMidiSysEx();
}

void ChildProcessSecurityPolicyImpl::GrantOrigin(int child_id,
                                                 const url::Origin& origin) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantOrigin(origin);
}

void ChildProcessSecurityPolicyImpl::GrantScheme(int child_id,
                                                 const std::string& scheme) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantScheme(scheme);
}

void ChildProcessSecurityPolicyImpl::GrantWebUIBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(BINDINGS_POLICY_WEB_UI);

  // Web UI bindings need the ability to request chrome: URLs.
  state->second->GrantScheme(kChromeUIScheme);

  // Web UI pages can contain links to file:// URLs.
  state->second->GrantScheme(url::kFileScheme);
}

void ChildProcessSecurityPolicyImpl::GrantReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantReadRawCookies();
}

void ChildProcessSecurityPolicyImpl::RevokeReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeReadRawCookies();
}

bool ChildProcessSecurityPolicyImpl::CanRequestURL(
    int child_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

  const std::string& scheme = url.scheme();

  if (IsPseudoScheme(scheme)) {
    // Every child process can request <about:blank>, <about:blank?foo>,
    // <about:blank/#foo> and <about:srcdoc>.
    if (url.IsAboutBlank() || url == kAboutSrcDocURL)
      return true;
    // URLs like <about:version>, <about:crash>, <view-source:...> shouldn't be
    // requestable by any child process.  Also, this case covers
    // <javascript:...>, which should be handled internally by the process and
    // not kicked up to the browser.
    return false;
  }

  // Blob and filesystem URLs require special treatment, since they embed an
  // inner origin.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return false;

    url::Origin origin(url);
    return origin.unique() || IsWebSafeScheme(origin.scheme()) ||
           CanCommitURL(child_id, GURL(origin.Serialize()));
  }

  if (IsWebSafeScheme(scheme))
    return true;

  // If the process can commit the URL, it can request it.
  if (CanCommitURL(child_id, url))
    return true;

  // Also allow URLs destined for ShellExecute and not the browser itself.
  return !GetContentClient()->browser()->IsHandledURL(url) &&
         !net::URLRequest::IsHandledURL(url);
}

bool ChildProcessSecurityPolicyImpl::CanRedirectToURL(const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't redirect to invalid URLs.

  const std::string& scheme = url.scheme();

  // Can't redirect to error pages.
  if (scheme == kChromeErrorScheme)
    return false;

  if (IsPseudoScheme(scheme)) {
    // Redirects to a pseudo scheme (about, javascript, view-source, ...) are
    // not allowed. An exception is made for <about:blank> and its variations.
    return url.IsAboutBlank();
  }

  // Note about redirects and special URLs:
  // * data-url: Blocked by net::DataProtocolHandler::IsSafeRedirectTarget().
  // Depending on their inner origins and if the request is browser-initiated or
  // renderer-initiated, blob-urls and filesystem-urls might get blocked by
  // CanCommitURL or in DocumentLoader::RedirectReceived.
  // * blob-url: If not blocked, a 'file not found' response will be
  //             generated in net::BlobURLRequestJob::DidStart().
  // * filesystem-url: If not blocked, the response is displayed.

  return true;
}

bool ChildProcessSecurityPolicyImpl::CanCommitURL(int child_id,
                                                  const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't commit invalid URLs.

  const std::string& scheme = url.scheme();

  // Of all the pseudo schemes, only about:blank and about:srcdoc are allowed to
  // commit.
  if (IsPseudoScheme(scheme))
    return url == url::kAboutBlankURL || url == kAboutSrcDocURL;

  // Blob and filesystem URLs require special treatment; validate the inner
  // origin they embed.
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    if (IsMalformedBlobUrl(url))
      return false;

    url::Origin origin(url);
    return origin.unique() || CanCommitURL(child_id, GURL(origin.Serialize()));
  }

  {
    base::AutoLock lock(lock_);

    // Most schemes can commit in any process. Note that we check
    // schemes_okay_to_commit_in_any_process_ here, which is stricter than
    // IsWebSafeScheme().
    //
    // TODO(creis, nick): https://crbug.com/515309: in generalized Site
    // Isolation and/or --site-per-process, there will be no such thing as a
    // scheme that is okay to commit in any process. Instead, an URL from a site
    // that is isolated may only be committed in a process dedicated to that
    // site, so CanCommitURL will need to rely on explicit, per-process grants.
    // Note how today, even with extension isolation, the line below does not
    // enforce that http pages cannot commit in an extension process.
    if (base::ContainsKey(schemes_okay_to_commit_in_any_process_, scheme))
      return true;

    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to commit the URL.
    return state->second->CanCommitURL(url);
  }
}

bool ChildProcessSecurityPolicyImpl::CanSetAsOriginHeader(int child_id,
                                                          const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't set invalid URLs as origin headers.

  const std::string& scheme = url.scheme();

  // Suborigin URLs are a special case and are allowed to be an origin header.
  if (scheme == url::kHttpSuboriginScheme ||
      scheme == url::kHttpsSuboriginScheme) {
    DCHECK(IsPseudoScheme(scheme));
    return true;
  }

  // about:srcdoc cannot be used as an origin
  if (url == kAboutSrcDocURL)
    return false;

  // If this process can commit |url|, it can use |url| as an origin for
  // outbound requests.
  if (CanCommitURL(child_id, url))
    return true;

  // Allow schemes which may come from scripts executing in isolated worlds;
  // XHRs issued by such scripts reflect the script origin rather than the
  // document origin.
  {
    base::AutoLock lock(lock_);
    if (base::ContainsKey(schemes_okay_to_appear_as_origin_headers_, scheme))
      return true;
  }
  return false;
}

bool ChildProcessSecurityPolicyImpl::CanReadFile(int child_id,
                                                 const base::FilePath& file) {
  return HasPermissionsForFile(child_id, file, READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadAllFiles(
    int child_id,
    const std::vector<base::FilePath>& files) {
  return std::all_of(files.begin(), files.end(),
                     [this, child_id](const base::FilePath& file) {
                       return CanReadFile(child_id, file);
                     });
}

bool ChildProcessSecurityPolicyImpl::CanReadRequestBody(
    int child_id,
    const storage::FileSystemContext* file_system_context,
    const scoped_refptr<ResourceRequestBody>& body) {
  if (!body)
    return true;

  for (const ResourceRequestBody::Element& element : *body->elements()) {
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_FILE:
        if (!CanReadFile(child_id, element.path()))
          return false;
        break;

      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        if (!CanReadFileSystemFile(child_id, file_system_context->CrackURL(
                                                 element.filesystem_url())))
          return false;
        break;

      case ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY:
        // TYPE_DISK_CACHE_ENTRY can't be sent via IPC according to
        // content/common/resource_messages.cc
        NOTREACHED();
        return false;

      case ResourceRequestBody::Element::TYPE_BYTES:
      case ResourceRequestBody::Element::TYPE_BYTES_DESCRIPTION:
        // Data is self-contained within |body| - no need to check access.
        break;

      case ResourceRequestBody::Element::TYPE_BLOB:
        // No need to validate - the unguessability of the uuid of the blob is a
        // sufficient defense against access from an unrelated renderer.
        break;

      case ResourceRequestBody::Element::TYPE_UNKNOWN:
      default:
        // Fail safe - deny access.
        NOTREACHED();
        return false;
    }
  }
  return true;
}

bool ChildProcessSecurityPolicyImpl::CanReadRequestBody(
    SiteInstance* site_instance,
    const scoped_refptr<ResourceRequestBody>& body) {
  DCHECK(site_instance);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int child_id = site_instance->GetProcess()->GetID();

  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      site_instance->GetBrowserContext(), site_instance);
  const storage::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();

  return CanReadRequestBody(child_id, file_system_context, body);
}

bool ChildProcessSecurityPolicyImpl::CanCreateReadWriteFile(
    int child_id,
    const base::FilePath& file) {
  return HasPermissionsForFile(child_id, file, CREATE_READ_WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id, READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     READ_FILE_GRANT | WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCopyIntoFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     COPY_INTO_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanDeleteFromFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id, filesystem_id,
                                     DELETE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  base::AutoLock lock(lock_);
  bool result = ChildProcessHasPermissionsForFile(child_id, file, permissions);
  if (!result) {
    // If this is a worker thread that has no access to a given file,
    // let's check that its renderer process has access to that file instead.
    WorkerToMainProcessMap::iterator iter = worker_map_.find(child_id);
    if (iter != worker_map_.end() && iter->second != 0) {
      result = ChildProcessHasPermissionsForFile(iter->second,
                                                 file,
                                                 permissions);
    }
  }
  return result;
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url,
    int permissions) {
  if (!filesystem_url.is_valid())
    return false;

  if (filesystem_url.path().ReferencesParent())
    return false;

  // Any write access is disallowed on the root path.
  if (storage::VirtualPath::IsRootPath(filesystem_url.path()) &&
      (permissions & ~READ_FILE_GRANT)) {
    return false;
  }

  if (filesystem_url.mount_type() == storage::kFileSystemTypeIsolated) {
    // When Isolated filesystems is overlayed on top of another filesystem,
    // its per-filesystem permission overrides the underlying filesystem
    // permissions).
    return HasPermissionsForFileSystem(
        child_id, filesystem_url.mount_filesystem_id(), permissions);
  }

  // If |filesystem_url.origin()| is not committable in this process, then this
  // page should not be able to place content in that origin via the filesystem
  // API either.
  if (!CanCommitURL(child_id, filesystem_url.origin())) {
    UMA_HISTOGRAM_BOOLEAN("FileSystem.OriginFailedCanCommitURL", true);
    return false;
  }

  FileSystemPermissionPolicyMap::iterator found =
      file_system_policy_map_.find(filesystem_url.type());
  if (found == file_system_policy_map_.end())
    return false;

  if ((found->second & storage::FILE_PERMISSION_READ_ONLY) &&
      permissions & ~READ_FILE_GRANT) {
    return false;
  }

  if (found->second & storage::FILE_PERMISSION_USE_FILE_PERMISSION)
    return HasPermissionsForFile(child_id, filesystem_url.path(), permissions);

  if (found->second & storage::FILE_PERMISSION_SANDBOX)
    return true;

  return false;
}

bool ChildProcessSecurityPolicyImpl::CanReadFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         READ_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCreateFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         CREATE_NEW_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCreateReadWriteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         CREATE_READ_WRITE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanCopyIntoFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         COPY_INTO_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::CanDeleteFileSystemFile(
    int child_id,
    const storage::FileSystemURL& filesystem_url) {
  return HasPermissionsForFileSystemFile(child_id, filesystem_url,
                                         DELETE_FILE_GRANT);
}

bool ChildProcessSecurityPolicyImpl::HasWebUIBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_web_ui_bindings();
}

bool ChildProcessSecurityPolicyImpl::CanReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->can_read_raw_cookies();
}

void ChildProcessSecurityPolicyImpl::AddChild(int child_id) {
  if (security_state_.count(child_id) != 0) {
    NOTREACHED() << "Add child process at most once.";
    return;
  }

  security_state_[child_id] = base::MakeUnique<SecurityState>();
}

bool ChildProcessSecurityPolicyImpl::ChildProcessHasPermissionsForFile(
    int child_id, const base::FilePath& file, int permissions) {
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFile(file, permissions);
}

bool ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(int child_id,
                                                            const GURL& url) {
  // It's important to call GetSiteForURL before acquiring |lock_|, since
  // GetSiteForURL consults IsIsolatedOrigin, which needs to grab the same
  // lock.
  //
  // TODO(creis): We must pass the valid browser_context to convert hosted apps
  // URLs. Currently, hosted apps cannot set cookies in this mode. See
  // http://crbug.com/160576.
  GURL site_url = SiteInstanceImpl::GetSiteForURL(NULL, url);

  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end()) {
    // TODO(nick): Returning true instead of false here is a temporary
    // workaround for https://crbug.com/600441
    return true;
  }
  return state->second->CanAccessDataForOrigin(site_url);
}

bool ChildProcessSecurityPolicyImpl::HasSpecificPermissionForOrigin(
    int child_id,
    const url::Origin& origin) {
  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->CanCommitOrigin(origin);
}

void ChildProcessSecurityPolicyImpl::LockToOrigin(int child_id,
                                                  const GURL& gurl) {
  // "gurl" can be currently empty in some cases, such as file://blah.
  DCHECK(SiteInstanceImpl::GetSiteForURL(NULL, gurl) == gurl);
  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  DCHECK(state != security_state_.end());
  state->second->LockToOrigin(gurl);
}

ChildProcessSecurityPolicyImpl::CheckOriginLockResult
ChildProcessSecurityPolicyImpl::CheckOriginLock(int child_id,
                                                const GURL& site_url) {
  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return ChildProcessSecurityPolicyImpl::CheckOriginLockResult::NO_LOCK;
  return state->second->CheckOriginLock(site_url);
}

void ChildProcessSecurityPolicyImpl::GrantPermissionsForFileSystem(
    int child_id,
    const std::string& filesystem_id,
    int permission) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;
  state->second->GrantPermissionsForFileSystem(filesystem_id, permission);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystem(
    int child_id,
    const std::string& filesystem_id,
    int permission) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFileSystem(filesystem_id, permission);
}

void ChildProcessSecurityPolicyImpl::RegisterFileSystemPermissionPolicy(
    storage::FileSystemType type,
    int policy) {
  base::AutoLock lock(lock_);
  file_system_policy_map_[type] = policy;
}

bool ChildProcessSecurityPolicyImpl::CanSendMidiSysExMessage(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->can_send_midi_sysex();
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOrigin(
    const url::Origin& origin) {
  CHECK(IsolatedOriginUtil::IsValidIsolatedOrigin(origin));

  base::AutoLock lock(lock_);
  CHECK(!isolated_origins_.count(origin))
      << "Duplicate isolated origin: " << origin.Serialize();

  isolated_origins_.insert(origin);
}

void ChildProcessSecurityPolicyImpl::AddIsolatedOriginsFromCommandLine(
    const std::string& origin_list) {
  for (const base::StringPiece& origin_piece :
       base::SplitStringPiece(origin_list, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    url::Origin origin((GURL(origin_piece)));
    if (!origin.unique())
      AddIsolatedOrigin(origin);
  }
}

bool ChildProcessSecurityPolicyImpl::IsIsolatedOrigin(
    const url::Origin& origin) {
  url::Origin unused_result;
  return GetMatchingIsolatedOrigin(origin, &unused_result);
}

bool ChildProcessSecurityPolicyImpl::GetMatchingIsolatedOrigin(
    const url::Origin& origin,
    url::Origin* result) {
  *result = url::Origin();
  base::AutoLock lock(lock_);

  // If multiple isolated origins are registered with a common domain suffix,
  // return the most specific one.  For example, if foo.isolated.com and
  // isolated.com are both isolated origins, bar.foo.isolated.com should return
  // foo.isolated.com.
  bool found = false;
  for (auto isolated_origin : isolated_origins_) {
    if (IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin(origin,
                                                          isolated_origin)) {
      if (!found || result->host().length() < isolated_origin.host().length()) {
        *result = isolated_origin;
        found = true;
      }
    }
  }

  return found;
}

void ChildProcessSecurityPolicyImpl::RemoveIsolatedOriginForTesting(
    const url::Origin& origin) {
  base::AutoLock lock(lock_);
  isolated_origins_.erase(origin);
}

}  // namespace content
