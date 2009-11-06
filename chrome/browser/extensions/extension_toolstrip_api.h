// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_shelf_model.h"

class Profile;

namespace extension_toolstrip_api_events {
  extern const char kOnToolstripExpanded[];
  extern const char kOnToolstripCollapsed[];
};  // namespace extension_toolstrip_api_events

class ToolstripFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl();

  ExtensionShelfModel* model_;
  ExtensionShelfModel::iterator toolstrip_;
};

class ToolstripExpandFunction : public ToolstripFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("toolstrip.expand")
};

class ToolstripCollapseFunction : public ToolstripFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("toolstrip.collapse")
};

class ToolstripEventRouter {
 public:
  // Toolstrip events.
  static void OnToolstripExpanded(Profile* profile,
                                  int routing_id,
                                  const GURL& url,
                                  int height);
  static void OnToolstripCollapsed(Profile* profile,
                                   int routing_id,
                                   const GURL& url);

 private:
  // Helper to actually dispatch an event to extension listeners.
  static void DispatchEvent(Profile* profile,
                            int routing_id,
                            const char* event_name,
                            const Value& json);

  DISALLOW_COPY_AND_ASSIGN(ToolstripEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_
