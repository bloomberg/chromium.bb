// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_security_policy_impl.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/isolated_context.h"

namespace content {

namespace {

const int kReadFilePermissions =
    base::PLATFORM_FILE_OPEN |
    base::PLATFORM_FILE_READ |
    base::PLATFORM_FILE_EXCLUSIVE_READ |
    base::PLATFORM_FILE_ASYNC;

const int kWriteFilePermissions =
    base::PLATFORM_FILE_OPEN |
    base::PLATFORM_FILE_WRITE |
    base::PLATFORM_FILE_EXCLUSIVE_WRITE |
    base::PLATFORM_FILE_ASYNC |
    base::PLATFORM_FILE_WRITE_ATTRIBUTES;

const int kEnumerateDirectoryPermissions =
    kReadFilePermissions |
    base::PLATFORM_FILE_ENUMERATE;

}  // namespace

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicyImpl::SecurityState {
 public:
  SecurityState()
    : enabled_bindings_(0),
      can_read_raw_cookies_(false) { }

  ~SecurityState() {
    scheme_policy_.clear();
    fileapi::IsolatedContext* isolated_context =
        fileapi::IsolatedContext::GetInstance();
    for (FileSystemMap::iterator iter = filesystem_permissions_.begin();
         iter != filesystem_permissions_.end();
         ++iter) {
      isolated_context->RemoveReference(iter->first);
    }
    UMA_HISTOGRAM_COUNTS("ChildProcessSecurityPolicy.PerChildFilePermissions",
                         file_permissions_.size());
  }

  // Grant permission to request URLs with the specified scheme.
  void GrantScheme(const std::string& scheme) {
    scheme_policy_[scheme] = true;
  }

  // Revoke permission to request URLs with the specified scheme.
  void RevokeScheme(const std::string& scheme) {
    scheme_policy_[scheme] = false;
  }

  // Grant certain permissions to a file.
  void GrantPermissionsForFile(const FilePath& file, int permissions) {
    FilePath stripped = file.StripTrailingSeparators();
    file_permissions_[stripped] |= permissions;
    UMA_HISTOGRAM_COUNTS("ChildProcessSecurityPolicy.FilePermissionPathLength",
                         stripped.value().size());
  }

  // Grant navigation to a file but not the file:// scheme in general.
  void GrantRequestOfSpecificFile(const FilePath &file) {
    request_file_set_.insert(file.StripTrailingSeparators());
  }

  // Revokes all permissions granted to a file.
  void RevokeAllPermissionsForFile(const FilePath& file) {
    FilePath stripped = file.StripTrailingSeparators();
    file_permissions_.erase(stripped);
    request_file_set_.erase(stripped);
  }

  // Grant certain permissions to a file.
  void GrantPermissionsForFileSystem(const std::string& filesystem_id,
                                     int permissions) {
    if (filesystem_permissions_.find(filesystem_id) ==
        filesystem_permissions_.end())
      fileapi::IsolatedContext::GetInstance()->AddReference(filesystem_id);
    filesystem_permissions_[filesystem_id] = permissions;
  }

  bool HasPermissionsForFileSystem(const std::string& filesystem_id,
                                   int permissions) {
    if (filesystem_permissions_.find(filesystem_id) ==
        filesystem_permissions_.end())
      return false;
    return (filesystem_permissions_[filesystem_id] & permissions) ==
        permissions;
  }

  void GrantBindings(int bindings) {
    enabled_bindings_ |= bindings;
  }

  void GrantReadRawCookies() {
    can_read_raw_cookies_ = true;
  }

  void RevokeReadRawCookies() {
    can_read_raw_cookies_ = false;
  }

  // Determine whether permission has been granted to request |url|.
  bool CanRequestURL(const GURL& url) {
    // Having permission to a scheme implies permssion to all of its URLs.
    SchemeMap::const_iterator judgment(scheme_policy_.find(url.scheme()));
    if (judgment != scheme_policy_.end())
      return judgment->second;

    // file:// URLs are more granular.  The child may have been given
    // permission to a specific file but not the file:// scheme in general.
    if (url.SchemeIs(chrome::kFileScheme)) {
      FilePath path;
      if (net::FileURLToFilePath(url, &path))
        return request_file_set_.find(path) != request_file_set_.end();
    }

    return false;  // Unmentioned schemes are disallowed.
  }

  // Determine if the certain permissions have been granted to a file.
  bool HasPermissionsForFile(const FilePath& file, int permissions) {
    if (!permissions || file.empty() || !file.IsAbsolute())
      return false;
    FilePath current_path = file.StripTrailingSeparators();
    FilePath last_path;
    int skip = 0;
    while (current_path != last_path) {
      FilePath base_name =  current_path.BaseName();
      if (base_name.value() == FilePath::kParentDirectory) {
        ++skip;
      } else if (skip > 0) {
        if (base_name.value() != FilePath::kCurrentDirectory)
          --skip;
      } else {
        if (file_permissions_.find(current_path) != file_permissions_.end())
          return (file_permissions_[current_path] & permissions) == permissions;
      }
      last_path = current_path;
      current_path = current_path.DirName();
    }

    return false;
  }

  bool CanLoadPage(const GURL& gurl) {
    if (origin_lock_.is_empty())
      return true;

    // TODO(creis): We must pass the valid browser_context to convert hosted
    // apps URLs.  Currently, hosted apps cannot be loaded in this mode.
    // See http://crbug.com/160576.
    GURL site_gurl = SiteInstanceImpl::GetSiteForURL(NULL, gurl);
    return origin_lock_ == site_gurl;
  }

  bool CanAccessCookiesForOrigin(const GURL& gurl) {
    if (origin_lock_.is_empty())
      return true;
    // TODO(creis): We must pass the valid browser_context to convert hosted
    // apps URLs.  Currently, hosted apps cannot set cookies in this mode.
    // See http://crbug.com/160576.
    GURL site_gurl = SiteInstanceImpl::GetSiteForURL(NULL, gurl);
    return origin_lock_ == site_gurl;
  }

  bool CanSendCookiesForOrigin(const GURL& gurl) {
    // We only block cross-site cookies on network requests if the
    // --enable-strict-site-isolation flag is passed.  This is expected to break
    // compatibility with many sites.  The similar --site-per-process flag only
    // blocks JavaScript access to cross-site cookies (in
    // CanAccessCookiesForOrigin).
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kEnableStrictSiteIsolation))
      return true;

    if (origin_lock_.is_empty())
      return true;
    // TODO(creis): We must pass the valid browser_context to convert hosted
    // apps URLs.  Currently, hosted apps cannot set cookies in this mode.
    // See http://crbug.com/160576.
    GURL site_gurl = SiteInstanceImpl::GetSiteForURL(NULL, gurl);
    return origin_lock_ == site_gurl;
  }

  void LockToOrigin(const GURL& gurl) {
    origin_lock_ = gurl;
  }

  bool has_web_ui_bindings() const {
    return enabled_bindings_ & BINDINGS_POLICY_WEB_UI;
  }

  bool can_read_raw_cookies() const {
    return can_read_raw_cookies_;
  }

 private:
  typedef std::map<std::string, bool> SchemeMap;

  typedef int FilePermissionFlags;  // bit-set of PlatformFileFlags
  typedef std::map<FilePath, FilePermissionFlags> FileMap;
  typedef std::map<std::string, FilePermissionFlags> FileSystemMap;
  typedef std::set<FilePath> FileSet;

  // Maps URL schemes to whether permission has been granted or revoked:
  //   |true| means the scheme has been granted.
  //   |false| means the scheme has been revoked.
  // If a scheme is not present in the map, then it has never been granted
  // or revoked.
  SchemeMap scheme_policy_;

  // The set of files the child process is permited to upload to the web.
  FileMap file_permissions_;

  // The set of files the child process is permitted to load.
  FileSet request_file_set_;

  int enabled_bindings_;

  bool can_read_raw_cookies_;

  GURL origin_lock_;

  // The set of isolated filesystems the child process is permitted to access.
  FileSystemMap filesystem_permissions_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicyImpl::ChildProcessSecurityPolicyImpl() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(chrome::kHttpScheme);
  RegisterWebSafeScheme(chrome::kHttpsScheme);
  RegisterWebSafeScheme(chrome::kFtpScheme);
  RegisterWebSafeScheme(chrome::kDataScheme);
  RegisterWebSafeScheme("feed");
  RegisterWebSafeScheme(chrome::kBlobScheme);
  RegisterWebSafeScheme(chrome::kFileSystemScheme);

  // We know about the following pseudo schemes and treat them specially.
  RegisterPseudoScheme(chrome::kAboutScheme);
  RegisterPseudoScheme(chrome::kJavaScriptScheme);
  RegisterPseudoScheme(chrome::kViewSourceScheme);
}

ChildProcessSecurityPolicyImpl::~ChildProcessSecurityPolicyImpl() {
  web_safe_schemes_.clear();
  pseudo_schemes_.clear();
  STLDeleteContainerPairSecondPointers(security_state_.begin(),
                                       security_state_.end());
  security_state_.clear();
}

// static
ChildProcessSecurityPolicy* ChildProcessSecurityPolicy::GetInstance() {
  return ChildProcessSecurityPolicyImpl::GetInstance();
}

ChildProcessSecurityPolicyImpl* ChildProcessSecurityPolicyImpl::GetInstance() {
  return Singleton<ChildProcessSecurityPolicyImpl>::get();
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
  if (!security_state_.count(child_id))
    return;  // May be called multiple times.

  delete security_state_[child_id];
  security_state_.erase(child_id);
  worker_map_.erase(child_id);
}

void ChildProcessSecurityPolicyImpl::RegisterWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK(web_safe_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Web-safe implies not pseudo.";

  web_safe_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return (web_safe_schemes_.find(scheme) != web_safe_schemes_.end());
}

void ChildProcessSecurityPolicyImpl::RegisterPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(web_safe_schemes_.count(scheme) == 0) <<
      "Pseudo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicyImpl::IsPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);

  return (pseudo_schemes_.find(scheme) != pseudo_schemes_.end());
}

void ChildProcessSecurityPolicyImpl::RegisterDisabledSchemes(
    const std::set<std::string>& schemes) {
  base::AutoLock lock(lock_);
  disabled_schemes_ = schemes;
}

bool ChildProcessSecurityPolicyImpl::IsDisabledScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  return disabled_schemes_.find(scheme) != disabled_schemes_.end();
}

void ChildProcessSecurityPolicyImpl::GrantRequestURL(
    int child_id, const GURL& url) {

  if (!url.is_valid())
    return;  // Can't grant the capability to request invalid URLs.

  if (IsWebSafeScheme(url.scheme()))
    return;  // The scheme has already been whitelisted for every child process.

  if (IsPseudoScheme(url.scheme())) {
    // The view-source scheme is a special case of a pseudo-URL that eventually
    // results in requesting its embedded URL.
    if (url.SchemeIs(chrome::kViewSourceScheme)) {
      // URLs with the view-source scheme typically look like:
      //   view-source:http://www.google.com/a
      // In order to request these URLs, the child_id needs to be able to
      // request the embedded URL.
      GrantRequestURL(child_id, GURL(url.path()));
    }

    return;  // Can't grant the capability to request pseudo schemes.
  }

  {
    base::AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return;

    // When the child process has been commanded to request this scheme,
    // we grant it the capability to request all URLs of that scheme.
    state->second->GrantScheme(url.scheme());
  }
}

void ChildProcessSecurityPolicyImpl::GrantRequestSpecificFileURL(
    int child_id,
    const GURL& url) {
  if (!url.SchemeIs(chrome::kFileScheme))
    return;

  {
    base::AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return;

    // When the child process has been commanded to request a file:// URL,
    // then we grant it the capability for that URL only.
    FilePath path;
    if (net::FileURLToFilePath(url, &path))
        state->second->GrantRequestOfSpecificFile(path);
  }
}

void ChildProcessSecurityPolicyImpl::GrantReadFile(int child_id,
                                                   const FilePath& file) {
  GrantPermissionsForFile(child_id, file, kReadFilePermissions);
}

void ChildProcessSecurityPolicyImpl::GrantReadDirectory(
    int child_id, const FilePath& directory) {
  GrantPermissionsForFile(child_id, directory, kEnumerateDirectoryPermissions);
}

void ChildProcessSecurityPolicyImpl::GrantPermissionsForFile(
    int child_id, const FilePath& file, int permissions) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionsForFile(file, permissions);
}

void ChildProcessSecurityPolicyImpl::RevokeAllPermissionsForFile(
    int child_id, const FilePath& file) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeAllPermissionsForFile(file);
}

void ChildProcessSecurityPolicyImpl::GrantReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id, kReadFilePermissions);
}

void ChildProcessSecurityPolicyImpl::GrantReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  GrantPermissionsForFileSystem(child_id, filesystem_id,
                                kReadFilePermissions |
                                kWriteFilePermissions);
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
  state->second->GrantScheme(chrome::kChromeUIScheme);

  // Web UI pages can contain links to file:// URLs.
  state->second->GrantScheme(chrome::kFileScheme);
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

bool ChildProcessSecurityPolicyImpl::CanLoadPage(
    int child_id,
    const GURL& url,
    ResourceType::Type resource_type) {
  // If --site-per-process flag is passed, we should enforce
  // stronger security restrictions on page navigation.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess) &&
      ResourceType::IsFrame(resource_type)) {
    // TODO(irobert): This currently breaks some WebUI page such as
    // "chrome://extensions/" (belongs to site chrome://chrome/) which
    // will load an iframe for the page "chrome://uber-frame/"
    // (belongs to site chrome://uber-frame/).
    base::AutoLock lock(lock_);
    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;
    return state->second->CanLoadPage(url);
  }
  return true;
}

bool ChildProcessSecurityPolicyImpl::CanRequestURL(
    int child_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

  if (IsDisabledScheme(url.scheme()))
    return false; // The scheme is disabled by policy.

  if (IsWebSafeScheme(url.scheme()))
    return true;  // The scheme has been white-listed for every child process.

  if (IsPseudoScheme(url.scheme())) {
    // There are a number of special cases for pseudo schemes.

    if (url.SchemeIs(chrome::kViewSourceScheme)) {
      // A view-source URL is allowed if the child process is permitted to
      // request the embedded URL. Careful to avoid pointless recursion.
      GURL child_url(url.path());
      if (child_url.SchemeIs(chrome::kViewSourceScheme) &&
          url.SchemeIs(chrome::kViewSourceScheme))
          return false;

      return CanRequestURL(child_id, child_url);
    }

    if (LowerCaseEqualsASCII(url.spec(), chrome::kAboutBlankURL))
      return true;  // Every child process can request <about:blank>.

    // URLs like <about:memory> and <about:crash> shouldn't be requestable by
    // any child process.  Also, this case covers <javascript:...>, which should
    // be handled internally by the process and not kicked up to the browser.
    return false;
  }

  if (!GetContentClient()->browser()->IsHandledURL(url) &&
      !net::URLRequest::IsHandledURL(url)) {
    return true;  // This URL request is destined for ShellExecute.
  }

  {
    base::AutoLock lock(lock_);

    SecurityStateMap::iterator state = security_state_.find(child_id);
    if (state == security_state_.end())
      return false;

    // Otherwise, we consult the child process's security state to see if it is
    // allowed to request the URL.
    return state->second->CanRequestURL(url);
  }
}

bool ChildProcessSecurityPolicyImpl::CanReadFile(int child_id,
                                                 const FilePath& file) {
  return HasPermissionsForFile(child_id, file, kReadFilePermissions);
}

bool ChildProcessSecurityPolicyImpl::CanReadDirectory(
    int child_id, const FilePath& directory) {
  return HasPermissionsForFile(child_id,
                               directory,
                               kEnumerateDirectoryPermissions);
}

bool ChildProcessSecurityPolicyImpl::CanReadFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id,
                                     filesystem_id,
                                     kReadFilePermissions);
}

bool ChildProcessSecurityPolicyImpl::CanReadWriteFileSystem(
    int child_id, const std::string& filesystem_id) {
  return HasPermissionsForFileSystem(child_id,
                                     filesystem_id,
                                     kReadFilePermissions |
                                     kWriteFilePermissions);
}

bool ChildProcessSecurityPolicyImpl::HasPermissionsForFile(
    int child_id, const FilePath& file, int permissions) {
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

  security_state_[child_id] = new SecurityState();
}

bool ChildProcessSecurityPolicyImpl::ChildProcessHasPermissionsForFile(
    int child_id, const FilePath& file, int permissions) {
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->HasPermissionsForFile(file, permissions);
}

bool ChildProcessSecurityPolicyImpl::CanAccessCookiesForOrigin(
    int child_id, const GURL& gurl) {
  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->CanAccessCookiesForOrigin(gurl);
}

bool ChildProcessSecurityPolicyImpl::CanSendCookiesForOrigin(int child_id,
                                                             const GURL& gurl) {
  base::AutoLock lock(lock_);
  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;
  return state->second->CanSendCookiesForOrigin(gurl);
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

}  // namespace content
