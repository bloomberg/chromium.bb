// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_process_security_policy.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

static const int kReadFilePermissions =
    base::PLATFORM_FILE_OPEN |
    base::PLATFORM_FILE_READ |
    base::PLATFORM_FILE_EXCLUSIVE_READ |
    base::PLATFORM_FILE_ASYNC;

// The SecurityState class is used to maintain per-child process security state
// information.
class ChildProcessSecurityPolicy::SecurityState {
 public:
  SecurityState()
    : enabled_bindings_(0),
      can_read_raw_cookies_(false) { }
  ~SecurityState() {
    scheme_policy_.clear();
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
    file_permissions_[file.StripTrailingSeparators()] |= permissions;
  }

  // Revokes all permissions granted to a file.
  void RevokeAllPermissionsForFile(const FilePath& file) {
    file_permissions_.erase(file.StripTrailingSeparators());
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

  // Determine whether permission has been granted to request url.
  // Schemes that have not been granted default to being denied.
  bool CanRequestURL(const GURL& url) {
    SchemeMap::const_iterator judgment(scheme_policy_.find(url.scheme()));

    if (judgment == scheme_policy_.end())
      return false;  // Unmentioned schemes are disallowed.

    return judgment->second;
  }

  // Determine if the certain permissions have been granted to a file.
  bool HasPermissionsForFile(const FilePath& file, int permissions) {
    FilePath current_path = file.StripTrailingSeparators();
    FilePath last_path;
    while (current_path != last_path) {
      if (file_permissions_.find(current_path) != file_permissions_.end())
        return (file_permissions_[current_path] & permissions) == permissions;
      last_path = current_path;
      current_path = current_path.DirName();
    }

    return false;
  }

  bool has_dom_ui_bindings() const {
    return BindingsPolicy::is_dom_ui_enabled(enabled_bindings_);
  }

  bool has_extension_bindings() const {
    return BindingsPolicy::is_extension_enabled(enabled_bindings_);
  }

  bool can_read_raw_cookies() const {
    return can_read_raw_cookies_;
  }

 private:
  typedef std::map<std::string, bool> SchemeMap;
  typedef std::map<FilePath, int> FileMap;  // bit-set of PlatformFileFlags

  // Maps URL schemes to whether permission has been granted or revoked:
  //   |true| means the scheme has been granted.
  //   |false| means the scheme has been revoked.
  // If a scheme is not present in the map, then it has never been granted
  // or revoked.
  SchemeMap scheme_policy_;

  // The set of files the child process is permited to upload to the web.
  FileMap file_permissions_;

  int enabled_bindings_;

  bool can_read_raw_cookies_;

  DISALLOW_COPY_AND_ASSIGN(SecurityState);
};

ChildProcessSecurityPolicy::ChildProcessSecurityPolicy() {
  // We know about these schemes and believe them to be safe.
  RegisterWebSafeScheme(chrome::kHttpScheme);
  RegisterWebSafeScheme(chrome::kHttpsScheme);
  RegisterWebSafeScheme(chrome::kFtpScheme);
  RegisterWebSafeScheme(chrome::kDataScheme);
  RegisterWebSafeScheme("feed");
  RegisterWebSafeScheme(chrome::kExtensionScheme);
  RegisterWebSafeScheme(chrome::kBlobScheme);

  // We know about the following psuedo schemes and treat them specially.
  RegisterPseudoScheme(chrome::kAboutScheme);
  RegisterPseudoScheme(chrome::kJavaScriptScheme);
  RegisterPseudoScheme(chrome::kViewSourceScheme);
}

ChildProcessSecurityPolicy::~ChildProcessSecurityPolicy() {
  web_safe_schemes_.clear();
  pseudo_schemes_.clear();
  STLDeleteContainerPairSecondPointers(security_state_.begin(),
                                       security_state_.end());
  security_state_.clear();
}

// static
ChildProcessSecurityPolicy* ChildProcessSecurityPolicy::GetInstance() {
  return Singleton<ChildProcessSecurityPolicy>::get();
}

void ChildProcessSecurityPolicy::Add(int child_id) {
  base::AutoLock lock(lock_);
  if (security_state_.count(child_id) != 0) {
    NOTREACHED() << "Add child process at most once.";
    return;
  }

  security_state_[child_id] = new SecurityState();
}

void ChildProcessSecurityPolicy::Remove(int child_id) {
  base::AutoLock lock(lock_);
  if (!security_state_.count(child_id))
    return;  // May be called multiple times.

  delete security_state_[child_id];
  security_state_.erase(child_id);
}

void ChildProcessSecurityPolicy::RegisterWebSafeScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK(web_safe_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Web-safe implies not psuedo.";

  web_safe_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicy::IsWebSafeScheme(const std::string& scheme) {
  base::AutoLock lock(lock_);

  return (web_safe_schemes_.find(scheme) != web_safe_schemes_.end());
}

void ChildProcessSecurityPolicy::RegisterPseudoScheme(
    const std::string& scheme) {
  base::AutoLock lock(lock_);
  DCHECK(pseudo_schemes_.count(scheme) == 0) << "Add schemes at most once.";
  DCHECK(web_safe_schemes_.count(scheme) == 0) <<
      "Psuedo implies not web-safe.";

  pseudo_schemes_.insert(scheme);
}

bool ChildProcessSecurityPolicy::IsPseudoScheme(const std::string& scheme) {
  base::AutoLock lock(lock_);

  return (pseudo_schemes_.find(scheme) != pseudo_schemes_.end());
}

void ChildProcessSecurityPolicy::GrantRequestURL(
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

    // If the child process has been commanded to request a scheme, then we
    // grant it the capability to request URLs of that scheme.
    state->second->GrantScheme(url.scheme());
  }
}

void ChildProcessSecurityPolicy::GrantReadFile(int child_id,
                                               const FilePath& file) {
  GrantPermissionsForFile(child_id, file, kReadFilePermissions);
}

void ChildProcessSecurityPolicy::GrantPermissionsForFile(
    int child_id, const FilePath& file, int permissions) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantPermissionsForFile(file, permissions);
}

void ChildProcessSecurityPolicy::RevokeAllPermissionsForFile(
    int child_id, const FilePath& file) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeAllPermissionsForFile(file);
}

void ChildProcessSecurityPolicy::GrantScheme(int child_id,
                                             const std::string& scheme) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantScheme(scheme);
}

void ChildProcessSecurityPolicy::GrantDOMUIBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(BindingsPolicy::DOM_UI);

  // Web UI bindings need the ability to request chrome: URLs.
  state->second->GrantScheme(chrome::kChromeUIScheme);

  // Web UI pages can contain links to file:// URLs.
  state->second->GrantScheme(chrome::kFileScheme);
}

void ChildProcessSecurityPolicy::GrantExtensionBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantBindings(BindingsPolicy::EXTENSION);
}

void ChildProcessSecurityPolicy::GrantReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->GrantReadRawCookies();
}

void ChildProcessSecurityPolicy::RevokeReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return;

  state->second->RevokeReadRawCookies();
}

bool ChildProcessSecurityPolicy::CanRequestURL(
    int child_id, const GURL& url) {
  if (!url.is_valid())
    return false;  // Can't request invalid URLs.

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

  if (!net::URLRequest::IsHandledURL(url))
    return true;  // This URL request is destined for ShellExecute.

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

bool ChildProcessSecurityPolicy::CanReadFile(int child_id,
                                             const FilePath& file) {
  return HasPermissionsForFile(child_id, file, kReadFilePermissions);
}

bool ChildProcessSecurityPolicy::HasPermissionsForFile(
    int child_id, const FilePath& file, int permissions) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->HasPermissionsForFile(file, permissions);
}

bool ChildProcessSecurityPolicy::HasDOMUIBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_dom_ui_bindings();
}

bool ChildProcessSecurityPolicy::HasExtensionBindings(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->has_extension_bindings();
}

bool ChildProcessSecurityPolicy::CanReadRawCookies(int child_id) {
  base::AutoLock lock(lock_);

  SecurityStateMap::iterator state = security_state_.find(child_id);
  if (state == security_state_.end())
    return false;

  return state->second->can_read_raw_cookies();
}
