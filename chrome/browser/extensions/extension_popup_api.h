// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/notification_registrar.h"

namespace gfx {
class Point;
}  // namespace gfx

class Profile;
class ExtensionPopup;

// This extension function shows a pop-up extension view.  It is asynchronous
// because the callback must be invoked only after the associated render
// process/view has been created and fully initialized.
class PopupShowFunction : public AsyncExtensionFunction,
                          public NotificationObserver {
 public:
  PopupShowFunction();

  virtual void Run();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.popup.show")

 private:
  // Computes the screen-space position of the frame-relative point in the
  // extension view that is requesting to display a popup.
  bool ConvertHostPointToScreen(gfx::Point* point);

  // NotificationObserver methods.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  NotificationRegistrar registrar_;

#if defined(TOOLKIT_VIEWS)
  // The pop-up view created by this function, saved for access during
  // event notification.  The pop-up is not owned by the PopupShowFunction
  // instance.
  ExtensionPopup* popup_;
#endif
};

// Event router class for events related to the chrome.popup.* set of APIs.
class PopupEventRouter {
 public:
  static void OnPopupClosed(Profile* profile,
                            int routing_id);
 private:
  DISALLOW_COPY_AND_ASSIGN(PopupEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_API_H_
