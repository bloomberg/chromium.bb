// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_logger_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

// The implementation for the chrome://invalidations page.
class InvalidationsMessageHandler
    : public content::WebUIMessageHandler,
      public invalidation::InvalidationLoggerObserver {
 public:
  InvalidationsMessageHandler();
  virtual ~InvalidationsMessageHandler();

  // Implementation of InvalidationLoggerObserver
  virtual void OnRegistration(const base::DictionaryValue& details) OVERRIDE;
  virtual void OnUnregistration(const base::DictionaryValue& details) OVERRIDE;
  virtual void OnStateChange(const syncer::InvalidatorState& newState) OVERRIDE;
  virtual void OnUpdateIds(const base::DictionaryValue& details) OVERRIDE;
  virtual void OnDebugMessage(const base::DictionaryValue& details) OVERRIDE;
  virtual void OnInvalidation(
      const syncer::ObjectIdInvalidationMap& newInvalidations) OVERRIDE;

  // Implementation of WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  void UpdateContent(const base::ListValue* args);
  void UIReady(const base::ListValue* args);

 private:
  // The pointer to the internal InvalidatorService InvalidationLogger.
  // Used to get the information necessary to display to the JS and to
  // register ourselves as Observers for any notifications.
  invalidation::InvalidationLogger* logger_;
  // Check to see if we are already registered as an observer
  bool isRegistered;
  DISALLOW_COPY_AND_ASSIGN(InvalidationsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_
