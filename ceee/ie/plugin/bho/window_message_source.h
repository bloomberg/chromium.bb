// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window message source implementation.
#ifndef CEEE_IE_PLUGIN_BHO_WINDOW_MESSAGE_SOURCE_H_
#define CEEE_IE_PLUGIN_BHO_WINDOW_MESSAGE_SOURCE_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

// A WindowMessageSource instance monitors keyboard and mouse messages on the
// same thread as the one that creates the instance, and fires events to those
// registered sinks.
//
// NOTE: (1) All the non-static methods are supposed to be called on the thread
//           that creates the instance.
//       (2) Multiple WindowMessageSource instances cannot live in the same
//           thread. Only the first one will be successfully initialized.
class WindowMessageSource {
 public:
  enum MessageType {
    // The destination of the message is the content window (or its descendants)
    // of a tab.
    TAB_CONTENT_WINDOW,
    // Otherwise.
    BROWSER_UI_SAME_THREAD
  };

  // The interface that event consumers have to implement.
  // NOTE: All the callback methods will be called on the thread that creates
  // the WindowMessageSource instance.
  class Sink {
   public:
    virtual ~Sink() {}
    // Called before a message is handled.
    virtual void OnHandleMessage(MessageType type, const MSG* message_info) {}
  };

  WindowMessageSource();
  virtual ~WindowMessageSource();

  bool Initialize();
  void TearDown();

  virtual void RegisterSink(Sink* sink);
  virtual void UnregisterSink(Sink* sink);

 private:
  // Hook procedure for WH_GETMESSAGE.
  static LRESULT CALLBACK GetMessageHookProc(int code,
                                             WPARAM wparam,
                                             LPARAM lparam);
  void OnHandleMessage(const MSG* message_info);

  // Hook procedure for WH_CALLWNDPROCRET.
  static LRESULT CALLBACK CallWndProcRetHookProc(int code,
                                                 WPARAM wparam,
                                                 LPARAM lparam);
  void OnWindowNcDestroy(HWND window);

  // Returns true if the specified window is the tab content window or one of
  // its descendants.
  bool IsWithinTabContentWindow(HWND window);

  // Adds an entry to the message_source_map_. Returns false if the item was
  // already present.
  static bool AddEntryToMap(DWORD thread_id, WindowMessageSource* source);
  // Retrieves an entry from the message_source_map_.
  static WindowMessageSource* GetEntryFromMap(DWORD thread_id);
  // Removes an entry from the message_source_map_.
  static void RemoveEntryFromMap(DWORD thread_id);

  // The thread that creates this object.
  const DWORD create_thread_id_;
  // Event consumers.
  std::vector<Sink*> sinks_;

  // The handle to the hook procedure of WH_GETMESSAGE.
  HHOOK get_message_hook_;
  // The handle to the hook procedure of WH_CALLWNDPROCRET.
  HHOOK call_wnd_proc_ret_hook_;

  // Caches the information about whether a given window is within the tab
  // content window or not.
  typedef std::map<HWND, bool> TabContentWindowMap;
  TabContentWindowMap tab_content_window_map_;

  // Maintains a map from thread IDs to their corresponding
  // WindowMessageSource instances.
  typedef std::map<DWORD, WindowMessageSource*> MessageSourceMap;
  static MessageSourceMap message_source_map_;

  // Used to protect access to the message_source_map_.
  static base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WindowMessageSource);
};

#endif  // CEEE_IE_PLUGIN_BHO_WINDOW_MESSAGE_SOURCE_H_
