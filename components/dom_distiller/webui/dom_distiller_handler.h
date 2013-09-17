// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_HANDLER_H_
#define COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace dom_distiller {

// Handler class for DOM Distiller page operations.
class DomDistillerHandler : public content::WebUIMessageHandler {
 public:
  DomDistillerHandler();
  virtual ~DomDistillerHandler();

  // content::WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestEntries" message. This synchronously requests the
  // list of entries and returns it to the front end.
  virtual void HandleRequestEntries(const ListValue* args);

 private:
  // Factory for the creating refs in callbacks.
  base::WeakPtrFactory<DomDistillerHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerHandler);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_HANDLER_H_
