// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "url/gurl.h"

namespace password_manager {

class PasswordManagerClient;

// Class for managing password reuse detection. Now it receives keystrokes and
// does nothing with them. TODO(crbug.com/657041): write other features of this
// class when they are implemented. This class is one per-tab.
class PasswordReuseDetectionManager : public PasswordReuseDetectorConsumer {
 public:
  explicit PasswordReuseDetectionManager(PasswordManagerClient* client);
  ~PasswordReuseDetectionManager() override;

  void DidNavigateMainFrame(const GURL& main_frame_url);
  void OnKeyPressed(const base::string16& text);

  // PasswordReuseDetectorConsumer
  void OnReuseFound(const base::string16& password,
                    const std::string& saved_domain,
                    int saved_passwords,
                    int number_matches) override;

 private:
  PasswordManagerClient* client_;
  base::string16 input_characters_;
  GURL main_frame_url_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetectionManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTION_MANAGER_H_
