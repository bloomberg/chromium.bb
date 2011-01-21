// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#define CHROME_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_

#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"

class FilePath;
class GURL;

// The ChildProcessSecurityPolicy class is used to grant and revoke security
// capabilities for child porcesses.  For example, it restricts whether a child
// process is permmitted to loaded file:// URLs based on whether the process
// has ever been commanded to load file:// URLs by the browser.
//
// ChildProcessSecurityPolicy is a singleton that may be used on any thread.
//
class ChildProcessSecurityPolicy {
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

  // Upon creation, child processes should register themselves by calling this
  // this method exactly once.
  void Add(int child_id);

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

  // Grant the child process the ability to use DOM UI Bindings.
  void GrantDOMUIBindings(int child_id);

  // Grant the child process the ability to use extension Bindings.
  void GrantExtensionBindings(int child_id);

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

  // Determines if certain permissions were granted for a file. |permissions|
  // must be a bit-set of base::PlatformFileFlags.
  bool HasPermissionsForFile(int child_id,
                             const FilePath& file,
                             int permissions);

  // Returns true if the specified child_id has been granted DOMUIBindings.
  // The browser should check this property before assuming the child process is
  // allowed to use DOMUIBindings.
  bool HasDOMUIBindings(int child_id);

  // Returns true if the specified child_id has been granted DOMUIBindings.
  // The browser should check this property before assuming the child process is
  // allowed to use extension bindings.
  bool HasExtensionBindings(int child_id);

  // Returns true if the specified child_id has been granted ReadRawCookies.
  bool CanReadRawCookies(int child_id);

 private:
  friend class ChildProcessSecurityPolicyInProcessBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyInProcessBrowserTest,
                           NoLeak);

  class SecurityState;

  typedef std::set<std::string> SchemeSet;
  typedef std::map<int, SecurityState*> SecurityStateMap;

  // Obtain an instance of ChildProcessSecurityPolicy via GetInstance().
  ChildProcessSecurityPolicy();
  friend struct DefaultSingletonTraits<ChildProcessSecurityPolicy>;

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

  // This map holds a SecurityState for each child process.  The key for the
  // map is the ID of the ChildProcessHost.  The SecurityState objects are
  // owned by this object and are protected by |lock_|.  References to them must
  // not escape this class.
  SecurityStateMap security_state_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSecurityPolicy);
};

#endif  // CHROME_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
