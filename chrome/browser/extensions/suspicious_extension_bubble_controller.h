// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_

#include <string>
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "extensions/common/extension.h"

class Browser;
class ExtensionService;

namespace extensions {

class SuspiciousExtensionBubble;

class SuspiciousExtensionBubbleController : public ProfileKeyedAPI {
 public:
  explicit SuspiciousExtensionBubbleController(Profile* profile);
  virtual ~SuspiciousExtensionBubbleController();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<
      SuspiciousExtensionBubbleController>* GetFactoryInstance();

  // Convenience method to get the SuspiciousExtensionBubbleController for a
  // profile.
  static SuspiciousExtensionBubbleController* Get(Profile* profile);

  // Check for suspicious extensions, returns true if found.
  bool HasSuspiciousExtensions();

  // Sets up the callbacks and shows the bubble.
  void Show(SuspiciousExtensionBubble* bubble);

  // Text for various UI labels shown in the bubble.
  base::string16 GetTitle();
  base::string16 GetMessageBody();
  base::string16 GetOverflowText(const base::string16& overflow_count);
  base::string16 GetLearnMoreLabel();
  base::string16 GetDismissButtonLabel();

  // Returns a vector of names of suspicious extensions found.
  std::vector<base::string16> GetSuspiciousExtensionNames();

  // Callbacks from bubble. Declared virtual for testing purposes.
  virtual void OnBubbleDismiss();
  virtual void OnLinkClicked();

 private:
  friend class ProfileKeyedAPIFactory<SuspiciousExtensionBubbleController>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SuspiciousExtensionBubbleController";
  }
  static const bool kServiceRedirectedInIncognito = true;

  // Mark all extensions found as acknowledged (don't need to warn about them
  // again).
  void AcknowledgeWipeout();

  // The list of suspicious extensions found. Reset at the beginning of each
  // call to FoundSuspiciousExtensions.
  ExtensionIdList suspicious_extensions_;

  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  // This object only checks once for suspicious extensions because the dataset
  // doesn't change after startup.
  bool has_notified_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousExtensionBubbleController);
};

template <>
void ProfileKeyedAPIFactory<
    SuspiciousExtensionBubbleController>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
