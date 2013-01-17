// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
#define CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_


#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/child_process_security_policy.h"
#include "webkit/glue/resource_type.h"

class FilePath;
class GURL;

namespace content {

class CONTENT_EXPORT ChildProcessSecurityPolicyImpl
    : NON_EXPORTED_BASE(public ChildProcessSecurityPolicy) {
 public:
  // Object can only be created through GetInstance() so the constructor is
  // private.
  virtual ~ChildProcessSecurityPolicyImpl();

  static ChildProcessSecurityPolicyImpl* GetInstance();

  // ChildProcessSecurityPolicy implementation.
  virtual void RegisterWebSafeScheme(const std::string& scheme) OVERRIDE;
  virtual bool IsWebSafeScheme(const std::string& scheme) OVERRIDE;
  virtual void RegisterDisabledSchemes(const std::set<std::string>& schemes)
      OVERRIDE;
  virtual void GrantPermissionsForFile(int child_id,
                                       const FilePath& file,
                                       int permissions) OVERRIDE;
  virtual void GrantReadFile(int child_id, const FilePath& file) OVERRIDE;
  virtual void GrantReadFileSystem(
      int child_id,
      const std::string& filesystem_id) OVERRIDE;
  virtual void GrantWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) OVERRIDE;
  virtual void GrantCreateFileForFileSystem(
      int child_id,
      const std::string& filesystem_id) OVERRIDE;
  virtual void GrantScheme(int child_id, const std::string& scheme) OVERRIDE;
  virtual bool CanReadFile(int child_id, const FilePath& file) OVERRIDE;
  virtual bool CanReadFileSystem(int child_id,
                                 const std::string& filesystem_id) OVERRIDE;
  virtual bool CanReadWriteFileSystem(
      int child_id,
      const std::string& filesystem_id) OVERRIDE;

  // Pseudo schemes are treated differently than other schemes because they
  // cannot be requested like normal URLs.  There is no mechanism for revoking
  // pseudo schemes.
  void RegisterPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as pseudo scheme.
  bool IsPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| is listed as a disabled scheme.
  bool IsDisabledScheme(const std::string& scheme);

  // Upon creation, child processes should register themselves by calling this
  // this method exactly once.
  void Add(int child_id);

  // Upon creation, worker thread child processes should register themselves by
  // calling this this method exactly once. Workers that are not shared will
  // inherit permissions from their parent renderer process identified with
  // |main_render_process_id|.
  void AddWorker(int worker_child_id, int main_render_process_id);

  // Upon destruction, child processess should unregister themselves by caling
  // this method exactly once.
  void Remove(int child_id);

  // Whenever the browser processes commands the child process to request a URL,
  // it should call this method to grant the child process the capability to
  // request the URL, along with permission to request all URLs of the same
  // scheme.
  void GrantRequestURL(int child_id, const GURL& url);

  // Whenever the browser process drops a file icon on a tab, it should call
  // this method to grant the child process the capability to request this one
  // file:// URL, but not all urls of the file:// scheme.
  void GrantRequestSpecificFileURL(int child_id, const GURL& url);

  // Grants the child process permission to enumerate all the files in
  // this directory and read those files.
  void GrantReadDirectory(int child_id, const FilePath& directory);

  // Revokes all permissions granted to the given file.
  void RevokeAllPermissionsForFile(int child_id, const FilePath& file);

  // Grant the child process the ability to use Web UI Bindings.
  void GrantWebUIBindings(int child_id);

  // Grant the child process the ability to read raw cookies.
  void GrantReadRawCookies(int child_id);

  // Revoke read raw cookies permission.
  void RevokeReadRawCookies(int child_id);

  // Before servicing a child process's request for a URL, the browser should
  // call this method to determine whether the process has the capability to
  // request the URL.
  bool CanRequestURL(int child_id, const GURL& url);

  // Returns true if the process is permitted to load pages from
  // the given origin in main frames or subframes.
  // Only might return false if --site-per-process flag is used.
  bool CanLoadPage(int child_id,
                   const GURL& url,
                   ResourceType::Type resource_type);

  // Before servicing a child process's request to enumerate a directory
  // the browser should call this method to check for the capability.
  bool CanReadDirectory(int child_id, const FilePath& directory);

  // Determines if certain permissions were granted for a file. |permissions|
  // must be a bit-set of base::PlatformFileFlags.
  bool HasPermissionsForFile(int child_id,
                             const FilePath& file,
                             int permissions);

  // Returns true if the specified child_id has been granted WebUIBindings.
  // The browser should check this property before assuming the child process is
  // allowed to use WebUIBindings.
  bool HasWebUIBindings(int child_id);

  // Returns true if the specified child_id has been granted ReadRawCookies.
  bool CanReadRawCookies(int child_id);

  // Returns true if the process is permitted to read and modify the cookies for
  // the given origin.  Does not affect cookies attached to or set by network
  // requests.
  // Only might return false if the very experimental
  // --enable-strict-site-isolation or --site-per-process flags are used.
  bool CanAccessCookiesForOrigin(int child_id, const GURL& gurl);

  // Returns true if the process is permitted to attach cookies to (or have
  // cookies set by) network requests.
  // Only might return false if the very experimental
  // --enable-strict-site-isolation or --site-per-process flags are used.
  bool CanSendCookiesForOrigin(int child_id, const GURL& gurl);

  // Sets the process as only permitted to use and see the cookies for the
  // given origin.
  // Only used if the very experimental --enable-strict-site-isolation or
  // --site-per-process flags are used.
  void LockToOrigin(int child_id, const GURL& gurl);

  // Grants access permission to the given isolated file system
  // identified by |filesystem_id|.  See comments for
  // ChildProcessSecurityPolicy::GrantReadFileSystem() for more details.
  void GrantPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

  // Determines if certain permissions were granted for a file fystem.
  // |permissions| must be a bit-set of base::PlatformFileFlags.
  bool HasPermissionsForFileSystem(
      int child_id,
      const std::string& filesystem_id,
      int permission);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, SecurityState*> SecurityStateMap;
  typedef std::map<int, int> WorkerToMainProcessMap;

  // Obtain an instance of ChildProcessSecurityPolicyImpl via GetInstance().
  ChildProcessSecurityPolicyImpl();
  friend struct DefaultSingletonTraits<ChildProcessSecurityPolicyImpl>;

  // Adds child process during registration.
  void AddChild(int child_id);

  // Determines if certain permissions were granted for a file to given child
  // process. |permissions| must be a bit-set of base::PlatformFileFlags.
  bool ChildProcessHasPermissionsForFile(int child_id,
                                         const FilePath& file,
                                         int permissions);

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  base::Lock lock_;

  // These schemes are white-listed for all child processes.  This set is
  // protected by |lock_|.
  SchemeSet web_safe_schemes_;

  // These schemes do not actually represent retrievable URLs.  For example,
  // the the URLs in the "about" scheme are aliases to other URLs.  This set is
  // protected by |lock_|.
  SchemeSet pseudo_schemes_;

  // These schemes are disabled by policy, and child processes are always
  // denied permission to request them. This overrides |web_safe_schemes_|.
  // This set is protected by |lock_|.
  SchemeSet disabled_schemes_;

  // This map holds a SecurityState for each child process.  The key for the
  // map is the ID of the ChildProcessHost.  The SecurityState objects are
  // owned by this object and are protected by |lock_|.  References to them must
  // not escape this class.
  SecurityStateMap security_state_;

  // This maps keeps the record of which js worker thread child process
  // corresponds to which main js thread child process.
  WorkerToMainProcessMap worker_map_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_IMPL_H_
