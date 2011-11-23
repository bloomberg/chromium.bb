// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#define CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_

#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"

class FilePath;
class GURL;

// The ChildProcessSecurityPolicy class is used to grant and revoke security
// capabilities for child processes.  For example, it restricts whether a child
// process is permitted to load file:// URLs based on whether the process
// has ever been commanded to load file:// URLs by the browser.
//
// ChildProcessSecurityPolicy is a singleton that may be used on any thread.
//
class CONTENT_EXPORT ChildProcessSecurityPolicy {
 public:
  // Object can only be created through GetInstance() so the constructor is
  // private.
  ~ChildProcessSecurityPolicy();

  // There is one global ChildProcessSecurityPolicy object for the entire
  // browser process.  The object returned by this method may be accessed on
  // any thread.
  static ChildProcessSecurityPolicy* GetInstance();

  // Web-safe schemes can be requested by any child process.  Once a web-safe
  // scheme has been registered, any child process can request URLs with
  // that scheme.  There is no mechanism for revoking web-safe schemes.
  void RegisterWebSafeScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as a web-safe scheme.
  bool IsWebSafeScheme(const std::string& scheme);

  // Pseudo schemes are treated differently than other schemes because they
  // cannot be requested like normal URLs.  There is no mechanism for revoking
  // pseudo schemes.
  void RegisterPseudoScheme(const std::string& scheme);

  // Returns true iff |scheme| has been registered as pseudo scheme.
  bool IsPseudoScheme(const std::string& scheme);

  // Sets the list of disabled schemes.
  // URLs using these schemes won't be loaded at all. The previous list of
  // schemes is overwritten. An empty |schemes| disables this feature.
  // Schemes listed as disabled take precedence over Web-safe schemes.
  void RegisterDisabledSchemes(const std::set<std::string>& schemes);

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
  // request the URL.
  void GrantRequestURL(int child_id, const GURL& url);

  // Whenever the user picks a file from a <input type="file"> element, the
  // browser should call this function to grant the child process the capability
  // to upload the file to the web.
  void GrantReadFile(int child_id, const FilePath& file);

  // Grants the child process permission to enumerate all the files in
  // this directory and read those files.
  void GrantReadDirectory(int child_id, const FilePath& directory);

  // Grants certain permissions to a file. |permissions| must be a bit-set of
  // base::PlatformFileFlags.
  void GrantPermissionsForFile(int child_id,
                               const FilePath& file,
                               int permissions);

  // Revokes all permissions granted to the given file.
  void RevokeAllPermissionsForFile(int child_id, const FilePath& file);

  // Grants the child process the capability to access URLs of the provided
  // scheme.
  void GrantScheme(int child_id, const std::string& scheme);

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

  // Before servicing a child process's request to upload a file to the web, the
  // browser should call this method to determine whether the process has the
  // capability to upload the requested file.
  bool CanReadFile(int child_id, const FilePath& file);

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

  // Returns true if the process is permitted to see and use the cookies for
  // the given origin.
  // Only might return false if the very experimental
  // --enable-strict-site-isolation is used.
  bool CanUseCookiesForOrigin(int child_id, const GURL& gurl);

  // Sets the process as only permitted to use and see the cookies for the
  // given origin.
  // Only used if the very experimental --enable-strict-site-isolation is used.
  void LockToOrigin(int child_id, const GURL& gurl);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, SecurityState*> SecurityStateMap;
  typedef std::map<int, int> WorkerToMainProcessMap;

  // Obtain an instance of ChildProcessSecurityPolicy via GetInstance().
  ChildProcessSecurityPolicy();
  friend struct DefaultSingletonTraits<ChildProcessSecurityPolicy>;

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

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicy);
};

#endif  // CONTENT_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
