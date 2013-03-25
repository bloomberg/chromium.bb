// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_constants.h"

namespace content {
namespace devtools {

namespace Inspector {
namespace targetCrashed {
  const char kName[] = "Inspector.targetCrashed";
}  // targetCrashed
}  // Inspector

namespace DOM {
namespace setFileInputFiles {
  const char kName[] = "DOM.setFileInputFiles";
  const char kParamFiles[] = "files";
}  // setFileInputFiles
}  // DOM

namespace Page {
namespace handleJavaScriptDialog {
  const char kName[] = "Page.handleJavaScriptDialog";
  const char kParamAccept[] = "accept";
  const char kParamPromptText[] = "promptText";
}  // handleJavaScriptDialog
namespace navigate {
  const char kName[] = "Page.navigate";
  const char kParamUrl[] = "url";
}  // navigate
}  // Page

}  // devtools
}  // content

