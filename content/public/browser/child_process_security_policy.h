// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"

class FilePath;

namespace content {

// The ChildProcessSecurityPolicy class is used to grant and revoke security
// capabilities for child processes.  For example, it restricts whether a child
// process is permitted to load file:// URLs based on whether the process
// has ever been commanded to load file:// URLs by the browser.
//
// ChildProcessSecurityPolicy is a singleton that may be used on any thread.
//
class ChildProcessSecurityPolicy {
 public:
  virtual ~ChildProcessSecurityPolicy() {}

  // There is one global ChildProcessSecurityPolicy object for the entire
  // browser process.  The object returned by this method may be accessed on
  // any thread.
  static CONTENT_EXPORT ChildProcessSecurityPolicy* GetInstance();

  // Web-safe schemes can be requested by any child process.  Once a web-safe
  // scheme has been registered, any child process can request URLs with
  // that scheme.  There is no mechanism for revoking web-safe schemes.
  virtual void RegisterWebSafeScheme(const std::string& scheme) = 0;

  // Returns true iff |scheme| has been registered as a web-safe scheme.
  virtual bool IsWebSafeScheme(const std::string& scheme) = 0;

  // Sets the list of disabled schemes.
  // URLs using these schemes won't be loaded at all. The previous list of
  // schemes is overwritten. An empty |schemes| disables this feature.
  // Schemes listed as disabled take precedence over Web-safe schemes.
  virtual void RegisterDisabledSchemes(
      const std::set<std::string>& schemes) = 0;

  // Grants certain permissions to a file. |permissions| must be a bit-set of
  // base::PlatformFileFlags.
  virtual void GrantPermissionsForFile(int child_id,
                                       const FilePath& file,
                                       int permissions) = 0;

  // Before servicing a child process's request to upload a file to the web, the
  // browser should call this method to determine whether the process has the
  // capability to upload the requested file.
  virtual bool CanReadFile(int child_id, const FilePath& file) = 0;

  // Whenever the user picks a file from a <input type="file"> element, the
  // browser should call this function to grant the child process the capability
  // to upload the file to the web.
  virtual void GrantReadFile(int child_id, const FilePath& file) = 0;

  // Grants access permission to the given filesystem_id.
  virtual void GrantAccessFileSystem(int child_id,
                                     const std::string& filesystem_id) = 0;

  // Grants the child process the capability to access URLs of the provided
  // scheme.
  virtual void GrantScheme(int child_id, const std::string& scheme) = 0;
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_SECURITY_POLICY_H_
