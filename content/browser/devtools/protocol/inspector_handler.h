// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

namespace content {
namespace devtools {
namespace inspector {

class InspectorHandler {
 public:
  InspectorHandler();
  virtual ~InspectorHandler();

  void SetClient(scoped_ptr<Client> client);

  void TargetCrashed();

 private:
  scoped_ptr<Client> client_;

  DISALLOW_COPY_AND_ASSIGN(InspectorHandler);
};

}  // namespace inspector
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_
