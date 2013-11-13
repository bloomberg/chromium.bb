// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_

// The constants in this file should be used instead manually constructing
// strings passed to and from DevTools protocol.
//
// There is a plan to generate this file from inspector.json automatically.
// Until then please feel free to add the constants here as needed.

namespace content {
namespace devtools {

extern const char kParamX[];
extern const char kParamY[];
extern const char kParamWidth[];
extern const char kParamHeight[];
extern const char kResult[];

namespace DOM {

  namespace setFileInputFiles {
    extern const char kName[];
    extern const char kParamFiles[];
  }  // setFileInputFiles

}  // DOM

namespace Input {

  extern const char kParamType[];
  extern const char kParamModifiers[];
  extern const char kParamTimestamp[];
  extern const char kParamDeviceSpace[];

  namespace dispatchMouseEvent {
    extern const char kName[];
    extern const char kParamX[];
    extern const char kParamY[];
    extern const char kParamButton[];
    extern const char kParamClickCount[];
  }  // dispatchMouseEvent

  namespace dispatchGestureEvent {
    extern const char kName[];
    extern const char kParamDeltaX[];
    extern const char kParamDeltaY[];
    extern const char kParamPinchScale[];
  }  // dispatchGestureEvent

}  // Input

namespace Inspector {

  namespace detached {
    extern const char kName[];
    extern const char kParamReason[];
  }  // detached

  namespace targetCrashed {
    extern const char kName[];
  }  // targetCrashed

}  // Inspector

namespace Page {

  extern const char kData[];
  extern const char kMetadata[];
  extern const char kParamDeviceScaleFactor[];
  extern const char kParamPageScaleFactor[];
  extern const char kParamPageScaleFactorMin[];
  extern const char kParamPageScaleFactorMax[];
  extern const char kParamOffsetBottom[];
  extern const char kParamOffsetTop[];
  extern const char kParamViewport[];

  namespace disable {
    extern const char kName[];
  }  // disable

  namespace handleJavaScriptDialog {
    extern const char kName[];
    extern const char kParamAccept[];
    extern const char kParamPromptText[];
  }  // handleJavaScriptDialog

  namespace navigate {
    extern const char kName[];
    extern const char kParamUrl[];
  }  // navigate

  namespace reload {
    extern const char kName[];
  }  // reload

  namespace getNavigationHistory {
    extern const char kName[];
    extern const char kResponseCurrentIndex[];
    extern const char kResponseEntries[];
    extern const char kResponseEntryId[];
    extern const char kResponseEntryURL[];
    extern const char kResponseEntryTitle[];
  }  // getNavigationHistory

  namespace navigateToHistoryEntry {
    extern const char kName[];
    extern const char kParamEntryId[];
  }  // navigateToHistoryEntry

  namespace captureScreenshot {
    extern const char kName[];
    extern const char kParamFormat[];
    extern const char kParamQuality[];
    extern const char kParamMaxWidth[];
    extern const char kParamMaxHeight[];
  }  // captureScreenshot

  namespace canScreencast {
    extern const char kName[];
  }  // canScreencast

  namespace startScreencast {
    extern const char kName[];
  }  // startScreencast

  namespace stopScreencast {
    extern const char kName[];
  }  // stopScreencast

  namespace screencastFrame {
    extern const char kName[];
  }  // screencastFrame

  namespace queryUsageAndQuota {
    extern const char kName[];
    extern const char kParamSecurityOrigin[];
    extern const char kResponseQuota[];
    extern const char kResponseUsage[];
  }  // queryUsageAndQuota

  namespace Quota {
    extern const char kItemTemporary[];
    extern const char kItemPersistent[];
  }  // Quota

  namespace Usage {
    extern const char kItemTemporary[];
    extern const char kItemPersistent[];
    extern const char kItemSyncable[];
  }  // Usage

  namespace UsageItem {
    namespace ID {
      extern const char kFilesystem[];
      extern const char kDatabase[];
      extern const char kAppcache[];
      extern const char kIndexedDatabase[];
    }  // ID
    extern const char kItemID[];
    extern const char kItemValue[];
  }  // UsageItem

  namespace screencastVisibilityChanged {
    extern const char kName[];
    extern const char kParamVisible[];
  }  // screencastVisibilityChanged
}  // Page

namespace Tracing {
  extern const char kName[];

  namespace start {
    extern const char kName[];
    extern const char kCategories[];
    extern const char kTraceOptions[];
  }  // start

  namespace end {
    extern const char kName[];
  }

  namespace tracingComplete {
    extern const char kName[];
  }

  namespace dataCollected {
    extern const char kName[];
    extern const char kValue[];
  }

}  // Tracing

namespace Worker {

  namespace disconnectedFromWorker {
    extern const char kName[];
  }  // disconnectedFromWorker

}  // Worker


namespace SystemInfo {
  extern const char kName[];

namespace getInfo {
  extern const char kName[];
}  // getInfo
}  // SystemInfo

}  // devtools
}  // content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_CONSTANTSH_
