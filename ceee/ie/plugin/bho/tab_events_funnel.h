// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Tab Events.

#ifndef CEEE_IE_PLUGIN_BHO_TAB_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_TAB_EVENTS_FUNNEL_H_

#include <ocidl.h>  // for READYSTATE

#include "base/ref_counted.h"
#include "ceee/ie/plugin/bho/events_funnel.h"

// Implements a set of methods to send tab related events to the Broker.
class TabEventsFunnel : public EventsFunnel {
 public:
  TabEventsFunnel() {}

  explicit TabEventsFunnel(IEventSender* client) : EventsFunnel(client) {}

  // Sends the tabs.onMoved event to the Broker.
  // @param tab_handle The HWND of the tab that moved.
  // @param window_id The identifier of the window containing the moving tab.
  // @param from_index The index from which the tab moved away.
  // @param to_index The index where the tab moved to.
  virtual HRESULT OnMoved(HWND tab_handle, int window_id,
                          int from_index, int to_index);

  // Sends the tabs.onRemoved event to the Broker.
  // @param tab_handle The identifier of the tab that was removed.
  virtual HRESULT OnRemoved(HWND tab_handle);

  // Sends the tabs.onSelectionChanged event to the Broker.
  // @param tab_handle The HWND of the tab was selected.
  // @param window_id The identifier of the window containing the selected tab.
  virtual HRESULT OnSelectionChanged(HWND tab_handle, int window_id);

  // Sends the tabs.onCreated :b+event to the Broker.
  // @param tab_handle The HWND of the tab that was created.
  // @param url The current URL of the page.
  // @param completed If true, the status of the page is completed, otherwise,
  //    it is any othe other status values.
  virtual HRESULT OnCreated(HWND tab_handle, BSTR url, bool completed);

  // Sends the tabs.onUpdated event to the Broker.
  // @param tab_handle The HWND of the tab that was updated.
  // @param url The [optional] url where the tab was navigated to.
  // @param ready_state The ready state of the tab.
  virtual HRESULT OnUpdated(HWND tab_handle, BSTR url,
                            READYSTATE ready_state);

  // Sends the private message to unmap a tab to its BHO. This is the last
  // message a BHO should send, as its tab_id will no longer be mapped afterward
  // and will assert if used.
  // @param tab_handle The HWND of the tab to unmap.
  // @param tab_id The id of the tab to unmap.
  virtual HRESULT OnTabUnmapped(HWND tab_handle, int tab_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(TabEventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_TAB_EVENTS_FUNNEL_H_
