// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_constants.h"

namespace content {
namespace devtools {

const char kParamX[] = "x";
const char kParamY[] = "y";
const char kParamWidth[] = "width";
const char kParamHeight[] = "height";
const char kResult[] = "result";

namespace DOM {

namespace setFileInputFiles {
  const char kName[] = "DOM.setFileInputFiles";
  const char kParamFiles[] = "files";
}  // setFileInputFiles

}  // DOM

namespace Input {

const char kParamType[] = "type";
const char kParamModifiers[] = "modifiers";
const char kParamTimestamp[] = "timestamp";
const char kParamDeviceSpace[] = "deviceSpace";

namespace dispatchMouseEvent {
  const char kName[] = "Input.dispatchMouseEvent";
  const char kParamButton[] = "button";
  const char kParamClickCount[] = "clickCount";
}  // dispatchMouseEvent

namespace dispatchGestureEvent {
  const char kName[] = "Input.dispatchGestureEvent";
  const char kParamDeltaX[] = "deltaX";
  const char kParamDeltaY[] = "deltaY";
  const char kParamPinchScale[] = "pinchScale";
}  // dispatchGestureEvent

}  // Input

namespace Inspector {

namespace detached {
  const char kName[] = "Inspector.detached";
  const char kParamReason[] = "reason";
}  // detached

namespace targetCrashed {
  const char kName[] = "Inspector.targetCrashed";
}  // targetCrashed

}  // Inspector

namespace Page {

const char kData[] = "data";
const char kMetadata[] = "metadata";
const char kParamDeviceScaleFactor[] = "deviceScaleFactor";
const char kParamPageScaleFactor[] = "pageScaleFactor";
const char kParamPageScaleFactorMin[] = "pageScaleFactorMin";
const char kParamPageScaleFactorMax[] = "pageScaleFactorMax";
const char kParamOffsetBottom[] = "offsetBottom";
const char kParamOffsetTop[] = "offsetTop";
const char kParamViewport[] = "viewport";

namespace disable {
  const char kName[] = "Page.disable";
}  // disable

namespace handleJavaScriptDialog {
  const char kName[] = "Page.handleJavaScriptDialog";
  const char kParamAccept[] = "accept";
  const char kParamPromptText[] = "promptText";
}  // handleJavaScriptDialog

namespace navigate {
  const char kName[] = "Page.navigate";
  const char kParamUrl[] = "url";
}  // navigate

namespace reload {
  const char kName[] = "Page.reload";
}  // reload

namespace getNavigationHistory {
  const char kName[] = "Page.getNavigationHistory";
  const char kResponseCurrentIndex[] = "currentIndex";
  const char kResponseEntries[] = "entries";
  const char kResponseEntryId[] = "id";
  const char kResponseEntryURL[] = "url";
  const char kResponseEntryTitle[] = "title";
}  // getNavigationHistory

namespace navigateToHistoryEntry {
  const char kName[] = "Page.navigateToHistoryEntry";
  const char kParamEntryId[] = "entryId";
}  // navigateToHistoryEntry

namespace captureScreenshot {
  const char kName[] = "Page.captureScreenshot";
  const char kParamFormat[] = "format";
  const char kParamQuality[] = "quality";
  const char kParamMaxWidth[] = "maxWidth";
  const char kParamMaxHeight[] = "maxHeight";
}  // captureScreenshot

namespace canScreencast {
  const char kName[] = "Page.canScreencast";
}  // canScreencast

namespace startScreencast {
  const char kName[] = "Page.startScreencast";
}  // startScreencast

namespace stopScreencast {
  const char kName[] = "Page.stopScreencast";
}  // stopScreencast

namespace screencastFrame {
  const char kName[] = "Page.screencastFrame";
}  // screencastFrame

namespace queryUsageAndQuota {
  const char kName[] = "Page.queryUsageAndQuota";
  const char kParamSecurityOrigin[] = "securityOrigin";
  extern const char kResponseQuota[] = "quota";
  extern const char kResponseUsage[] = "usage";
}  // queryUsageAndQuota

namespace Quota {
  extern const char kItemTemporary[] = "temporary";
  extern const char kItemPersistent[] = "persistent";
}  // Quota

namespace Usage {
  extern const char kItemTemporary[] = "temporary";
  extern const char kItemPersistent[] = "persistent";
  extern const char kItemSyncable[] = "syncable";
}  // Usage

namespace UsageItem {
  namespace ID {
    extern const char kFilesystem[] = "filesystem";
    extern const char kDatabase[] = "database";
    extern const char kAppcache[] = "appcache";
    extern const char kIndexedDatabase[] = "indexeddatabase";
  }  // ID
  extern const char kItemID[] = "id";
  extern const char kItemValue[] = "value";
}  // UsageItem

namespace screencastVisibilityChanged {
  const char kName[] = "Page.screencastVisibilityChanged";
  const char kParamVisible[] = "visible";
}  // screencastVisibilityChanged

}  // Page

namespace Worker {

namespace disconnectedFromWorker {
  const char kName[] = "Worker.disconnectedFromWorker";
}  // disconnectedFromWorker

}  // Worker

namespace Tracing {
  const char kName[] = "Tracing";

namespace start {
  const char kName[] = "Tracing.start";
  const char kCategories[] = "categories";
  const char kTraceOptions[] = "trace-options";
}  // start

namespace end {
  const char kName[] = "Tracing.end";
}

namespace tracingComplete {
  const char kName[] = "Tracing.tracingComplete";
}

namespace dataCollected {
  const char kName[] = "Tracing.dataCollected";
  const char kValue[] = "value";
}

}  // Tracing

namespace SystemInfo {
  const char kName[] = "SystemInfo";

namespace getInfo {
  const char kName[] = "SystemInfo.getInfo";
}  // getInfo
}  // SystemInfo

}  // devtools
}  // content

