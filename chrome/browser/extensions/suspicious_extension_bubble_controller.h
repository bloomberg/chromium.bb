// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_

#include <string>
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "extensions/common/extension.h"

class Browser;
class ExtensionService;

namespace extensions {

class SuspiciousExtensionBubble;

class SuspiciousExtensionBubbleController
    : public ProfileKeyedAPI,
      public ExtensionMessageBubbleController,
      public ExtensionMessageBubbleController::Delegate {
 public:
  explicit SuspiciousExtensionBubbleController(Profile* profile);
  virtual ~SuspiciousExtensionBubbleController();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<
      SuspiciousExtensionBubbleController>* GetFactoryInstance();

  // Convenience method to get the SuspiciousExtensionBubbleController
  // for a profile.
  static SuspiciousExtensionBubbleController* Get(Profile* profile);

  // ExtensionMessageBubbleController::Delegate methods.
  virtual bool ShouldIncludeExtension(const std::string& extension_id) OVERRIDE;
  virtual void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) OVERRIDE;
  virtual void PerformAction(const ExtensionIdList& list) OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetMessageBody() const OVERRIDE;
  virtual base::string16 GetOverflowText(
      const base::string16& overflow_count) const OVERRIDE;
  virtual base::string16 GetLearnMoreLabel() const OVERRIDE;
  virtual GURL GetLearnMoreUrl() const OVERRIDE;
  virtual base::string16 GetActionButtonLabel() const OVERRIDE;
  virtual base::string16 GetDismissButtonLabel() const OVERRIDE;
  virtual bool ShouldShowExtensionList() const OVERRIDE;
  virtual std::vector<base::string16> GetExtensions() OVERRIDE;
  virtual void LogExtensionCount(size_t count) OVERRIDE;
  virtual void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) OVERRIDE;
 private:
  friend class ProfileKeyedAPIFactory<
      SuspiciousExtensionBubbleController>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SuspiciousExtensionBubbleController";
  }
  static const bool kServiceRedirectedInIncognito = true;

  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousExtensionBubbleController);
};

template <>
void ProfileKeyedAPIFactory<
    SuspiciousExtensionBubbleController>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
