// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Web Navigation Events.

#ifndef CEEE_IE_PLUGIN_BHO_WEBNAVIGATION_EVENTS_FUNNEL_H_
#define CEEE_IE_PLUGIN_BHO_WEBNAVIGATION_EVENTS_FUNNEL_H_
#include <atlcomcli.h>

#include "base/time.h"
#include "ceee/ie/plugin/bho/events_funnel.h"

#include "toolband.h"  // NOLINT

// Implements a set of methods to send web navigation related events to the
// Broker.
class WebNavigationEventsFunnel : public EventsFunnel {
 public:
  WebNavigationEventsFunnel() {}

  // Sends the webNavigation.onBeforeNavigate event to the Broker.
  // @param tab_handle The window handle of the tab in which the navigation is
  //        about to occur.
  // @param url The URL of the navigation.
  // @param frame_id 0 indicates the navigation happens in the tab content
  //        window; positive value indicates navigation in a subframe.
  // @param request_id The ID of the request to retrieve the document of this
  //        navigation.
  // @param time_stamp The time when the browser was about to start the
  //        navigation.
  virtual HRESULT OnBeforeNavigate(CeeeWindowHandle tab_handle,
                                   BSTR url,
                                   int frame_id,
                                   int request_id,
                                   const base::Time& time_stamp);

  // Sends the webNavigation.onBeforeRetarget event to the Broker.
  // @param source_tab_handle The window handle of the tab in which the
  //        navigation is triggered.
  // @param source_url The URL of the document that is opening the new window.
  // @param target_url The URL to be opened in the new window.
  // @param time_stamp The time when the browser was about to create a new view.
  virtual HRESULT OnBeforeRetarget(CeeeWindowHandle source_tab_handle,
                                   BSTR source_url,
                                   BSTR target_url,
                                   const base::Time& time_stamp);

  // Sends the webNavigation.onCommitted event to the Broker.
  // @param tab_handle The window handle of the tab in which the navigation
  //        occurs.
  // @param url The URL of the navigation.
  // @param frame_id 0 indicates the navigation happens in the tab content
  //        window; positive value indicates navigation in a subframe.
  // @param transition_type Cause of the navigation.
  // @param transition_qualifiers Zero or more transition qualifiers delimited
  //        by "|".
  // @param time_stamp The time when the navigation was committed.
  virtual HRESULT OnCommitted(CeeeWindowHandle tab_handle,
                              BSTR url,
                              int frame_id,
                              const char* transition_type,
                              const char* transition_qualifiers,
                              const base::Time& time_stamp);

  // Sends the webNavigation.onCompleted event to the Broker.
  // @param tab_handle The window handle of the tab in which the navigation
  //        occurs.
  // @param url The URL of the navigation.
  // @param frame_id 0 indicates the navigation happens in the tab content
  //        window; positive value indicates navigation in a subframe.
  // @param time_stamp The time when the document finished loading.
  virtual HRESULT OnCompleted(CeeeWindowHandle tab_handle,
                              BSTR url,
                              int frame_id,
                              const base::Time& time_stamp);

  // Sends the webNavigation.onDOMContentLoaded event to the Broker.
  // @param tab_handle The window handle of the tab in which the navigation
  //        occurs.
  // @param url The URL of the navigation.
  // @param frame_id 0 indicates the navigation happens in the tab content
  //        window; positive value indicates navigation in a subframe.
  // @param time_stamp The time when the page's DOM was fully constructed.
  virtual HRESULT OnDOMContentLoaded(CeeeWindowHandle tab_handle,
                                     BSTR url,
                                     int frame_id,
                                     const base::Time& time_stamp);

  // Sends the webNavigation.onErrorOccurred event to the Broker.
  // @param tab_handle The window handle of the tab in which the navigation
  //        occurs.
  // @param url The URL of the navigation.
  // @param frame_id 0 indicates the navigation happens in the tab content
  //        window; positive value indicates navigation in a subframe.
  // @param error The error description.
  // @param time_stamp The time when the error occurred.
  virtual HRESULT OnErrorOccurred(CeeeWindowHandle tab_handle,
                                  BSTR url,
                                  int frame_id,
                                  BSTR error,
                                  const base::Time& time_stamp);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNavigationEventsFunnel);
};

#endif  // CEEE_IE_PLUGIN_BHO_WEBNAVIGATION_EVENTS_FUNNEL_H_
