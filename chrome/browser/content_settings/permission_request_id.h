// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_

#include <string>

// Uniquely identifies a particular permission request.
class PermissionRequestID {
 public:
  PermissionRequestID(int render_process_id,
                      int render_view_id,
                      int bridge_id,
                      int group_id);
  ~PermissionRequestID();

  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }
  int bridge_id() const { return bridge_id_; }
  int group_id() const { return group_id_; }

  bool Equals(const PermissionRequestID& other) const;
  bool IsForSameTabAs(const PermissionRequestID& other) const;
  std::string ToString() const;

 private:
  int render_process_id_;
  int render_view_id_;
  // Id unique to this instance.
  int bridge_id_;
  // Id possibly shared by multiple instance. This is used in
  // PermissionQueueController::CancelInfoBarRequests(int group_id) to cancel
  // multiple permission requests by a group id.
  // For example, ProtectedMediaIdentifierPermissionContext class uses this.
  // If you don't use that function, this doesn't matter and setting this to 0
  // is recommended.
  int group_id_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_
