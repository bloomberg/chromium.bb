// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_test_factory.h"

#include "content/test/plugin/plugin_arguments_test.h"
#include "content/test/plugin/plugin_delete_plugin_in_stream_test.h"
#include "content/test/plugin/plugin_delete_plugin_in_deallocate_test.h"
#include "content/test/plugin/plugin_get_javascript_url_test.h"
#include "content/test/plugin/plugin_get_javascript_url2_test.h"
#include "content/test/plugin/plugin_geturl_test.h"
#include "content/test/plugin/plugin_javascript_open_popup.h"
#include "content/test/plugin/plugin_new_fails_test.h"
#include "content/test/plugin/plugin_npobject_identity_test.h"
#include "content/test/plugin/plugin_npobject_lifetime_test.h"
#include "content/test/plugin/plugin_npobject_proxy_test.h"
#include "content/test/plugin/plugin_private_test.h"
#include "content/test/plugin/plugin_request_read_test.h"
#include "content/test/plugin/plugin_schedule_timer_test.h"
#include "content/test/plugin/plugin_setup_test.h"
#include "content/test/plugin/plugin_thread_async_call_test.h"
#include "content/test/plugin/plugin_window_size_test.h"
#if defined(OS_WIN)
#include "content/test/plugin/plugin_windowed_test.h"
#endif
#include "content/test/plugin/plugin_windowless_test.h"

namespace NPAPIClient {

PluginTest* CreatePluginTest(const std::string& test_name,
                             NPP instance,
                             NPNetscapeFuncs* host_functions) {
  PluginTest* new_test = NULL;

  if (test_name == "arguments") {
    new_test = new PluginArgumentsTest(instance, host_functions);
  } else if (test_name == "geturl" || test_name == "geturl_404_response" ||
             test_name == "geturl_fail_write" ||
             test_name == "plugin_referrer_test" ||
             test_name == "geturlredirectnotify" ||
             test_name == "cookies") {
    new_test = new PluginGetURLTest(instance, host_functions);
  } else if (test_name == "npobject_identity") {
    new_test = new NPObjectIdentityTest(instance, host_functions);
  } else if (test_name == "npobject_proxy") {
    new_test = new NPObjectProxyTest(instance, host_functions);
  } else if (test_name == "invoke_js_function_on_create" ||
             test_name == "resize_during_paint"
#if defined(OS_WIN) || defined(OS_MACOSX)
          // TODO(port): plugin_windowless_test.*.
          || test_name == "execute_script_delete_in_paint" ||
             test_name == "execute_script_delete_in_mouse_up" ||
             test_name == "delete_frame_test" ||
             test_name == "multiple_instances_sync_calls" ||
             test_name == "no_hang_if_init_crashes" ||
             test_name == "convert_point"
#endif
             ) {
    new_test = new WindowlessPluginTest(instance, host_functions);
  } else if (test_name == "getjavascripturl") {
    new_test = new ExecuteGetJavascriptUrlTest(instance, host_functions);
  } else if (test_name == "getjavascripturl2") {
    new_test = new ExecuteGetJavascriptUrl2Test(instance, host_functions);
#if defined(OS_WIN)
  // TODO(port): plugin_window_size_test.*.
  } else if (test_name == "checkwindowrect") {
    new_test = new PluginWindowSizeTest(instance, host_functions);
#endif
  } else if (test_name == "self_delete_plugin_stream") {
    new_test = new DeletePluginInStreamTest(instance, host_functions);
#if defined(OS_WIN)
  // TODO(port): plugin_npobject_lifetime_test.*.
  } else if (test_name == "npobject_lifetime_test") {
    new_test = new NPObjectLifetimeTest(instance, host_functions);
  } else if (test_name == "npobject_lifetime_test_second_instance") {
    new_test = new NPObjectLifetimeTestInstance2(instance, host_functions);
  } else if (test_name == "new_fails") {
    new_test = new NewFailsTest(instance, host_functions);
  } else if (test_name == "npobject_delete_plugin_in_evaluate" ||
             test_name == "npobject_delete_create_plugin_in_evaluate") {
    new_test = new NPObjectDeletePluginInNPN_Evaluate(instance, host_functions);
#endif
  } else if (test_name == "plugin_javascript_open_popup_with_plugin") {
    new_test = new ExecuteJavascriptOpenPopupWithPluginTest(
        instance, host_functions);
  } else if (test_name == "plugin_popup_with_plugin_target") {
    new_test = new ExecuteJavascriptPopupWindowTargetPluginTest(
        instance, host_functions);
  } else if (test_name == "plugin_thread_async_call") {
    new_test = new PluginThreadAsyncCallTest(instance, host_functions);
  } else if (test_name == "private") {
    new_test = new PrivateTest(instance, host_functions);
  } else if (test_name == "schedule_timer") {
    new_test = new ScheduleTimerTest(instance, host_functions);
#if defined(OS_WIN)
  // TODO(port): plugin_windowed_test.*.
  } else if (test_name == "hidden_plugin" ||
             test_name == "create_instance_in_paint" ||
             test_name == "alert_in_window_message" ||
             test_name == "ensure_scripting_works_in_destroy" ||
             test_name == "set_title_in_paint" ||
             test_name == "set_title_in_set_window_and_paint") {
    new_test = new WindowedPluginTest(instance, host_functions);
#endif
  } else if (test_name == "setup") {
    // "plugin" is the name for plugin documents.
    new_test = new PluginSetupTest(instance, host_functions);
  } else if (test_name == "delete_plugin_in_deallocate_test") {
    new_test = new DeletePluginInDeallocateTest(instance, host_functions);
  } else if (test_name == "plugin_request_read_single_range") {
    new_test = new PluginRequestReadTest(instance, host_functions);
  }

  return new_test;
}

} // namespace NPAPIClient
