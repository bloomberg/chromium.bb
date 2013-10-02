// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_TYPE_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_TYPE_H_

namespace performance_monitor {

// IMPORTANT: To add new events, please
// - Place the new event above EVENT_NUMBER_OF_EVENTS.
// - Add a member to the EventKeyChar enum in key_builder.cc.
// - Add the appropriate messages in generated_resources.grd.
// - Add the appropriate functions in
//   chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h.
enum EventType {
  EVENT_UNDEFINED,

  // Extension-Related events
  EVENT_EXTENSION_INSTALL,
  EVENT_EXTENSION_UNINSTALL,
  EVENT_EXTENSION_UPDATE,
  EVENT_EXTENSION_ENABLE,
  EVENT_EXTENSION_DISABLE,

  // Chrome's version has changed.
  EVENT_CHROME_UPDATE,

  // Renderer-Failure related events; these correspond to the RENDERER_HANG
  // event, and the two termination statuses ABNORMAL_EXIT and PROCESS_KILLED,
  // respectively.
  EVENT_RENDERER_HANG,
  EVENT_RENDERER_CRASH,
  EVENT_RENDERER_KILLED,

  // Chrome did not shut down correctly.
  EVENT_UNCLEAN_EXIT,

  EVENT_NUMBER_OF_EVENTS
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_TYPE_H_
