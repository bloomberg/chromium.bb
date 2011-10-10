// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/common/extensions/extension.h"

class ExtensionService;

// This class encapsulates the UI we want to show users when certain events
// occur related to installed extensions.
class ExtensionGlobalError : public GlobalError {
 public:
  explicit ExtensionGlobalError(
      base::WeakPtr<ExtensionService> extension_service);
  virtual ~ExtensionGlobalError();

  // Inform us that a given extension is of a certain type that the user
  // hasn't yet acknowledged.
  void AddExternalExtension(const std::string& id);
  void AddBlacklistedExtension(const std::string& id);
  void AddOrphanedExtension(const std::string& id);

  // Returns sets replaying the IDs that have been added with the
  // Add[...]Extension methods.
  const ExtensionIdSet* get_external_extension_ids() const {
    return external_extension_ids_.get();
  }

  const ExtensionIdSet* get_blacklisted_extension_ids() const {
    return blacklisted_extension_ids_.get();
  }

  const ExtensionIdSet* get_orphaned_extension_ids() const {
    return orphaned_extension_ids_.get();
  }

  typedef base::Callback<void(const ExtensionGlobalError&)>
      ExtensionGlobalErrorCallback;

  // Called when the user presses the "Accept" button on the alert.
  void set_accept_callback(ExtensionGlobalErrorCallback callback);

  // Called when the user presses the "Cancel" button on the alert.
  void set_cancel_callback(ExtensionGlobalErrorCallback callback);

  // Called when the alert is dismissed with no direct user action
  // (e.g., the browser exits).
  void set_closed_callback(ExtensionGlobalErrorCallback callback);

  // GlobalError methods.
  virtual bool HasBadge() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void BubbleViewDidClose() OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE;
  virtual void BubbleViewCancelButtonPressed() OVERRIDE;

 private:
  base::WeakPtr<ExtensionService> extension_service_;
  scoped_ptr<ExtensionIdSet> external_extension_ids_;
  scoped_ptr<ExtensionIdSet> blacklisted_extension_ids_;
  scoped_ptr<ExtensionIdSet> orphaned_extension_ids_;
  ExtensionGlobalErrorCallback accept_callback_;
  ExtensionGlobalErrorCallback cancel_callback_;
  ExtensionGlobalErrorCallback closed_callback_;
  string16 message_;  // Displayed in the body of the alert.

  // For a given set of extension IDs, generates appropriate text
  // describing what the user needs to know about them.
  string16 GenerateMessageSection(const ExtensionIdSet* extensions,
                                  int template_message_id);

  // Generates the message displayed in the body of the alert.
  string16 GenerateMessage();

  DISALLOW_COPY_AND_ASSIGN(ExtensionGlobalError);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_H_
