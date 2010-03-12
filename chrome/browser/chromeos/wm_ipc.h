// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_IPC_H_
#define CHROME_BROWSER_CHROMEOS_WM_IPC_H_

#include <gtk/gtk.h>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"

typedef unsigned long Atom;
typedef unsigned long XID;

namespace chromeos {

class WmIpc {
 public:
  enum AtomType {
    ATOM_CHROME_WINDOW_TYPE = 0,
    ATOM_CHROME_WM_MESSAGE,
    ATOM_MANAGER,
    ATOM_NET_SUPPORTING_WM_CHECK,
    ATOM_NET_WM_NAME,
    ATOM_PRIMARY,
    ATOM_STRING,
    ATOM_UTF8_STRING,
    ATOM_WM_NORMAL_HINTS,
    ATOM_WM_S0,
    ATOM_WM_STATE,
    ATOM_WM_TRANSIENT_FOR,
    ATOM_WM_SYSTEM_METRICS,
    kNumAtoms,
  };

  enum WindowType {
    WINDOW_TYPE_UNKNOWN = 0,

    // A top-level Chrome window.
    WINDOW_TYPE_CHROME_TOPLEVEL,

    // A window showing scaled-down views of all of the tabs within a
    // Chrome window.
    WINDOW_TYPE_CHROME_TAB_SUMMARY,

    // A tab that's been detached from a Chrome window and is currently
    // being dragged.
    //   param[0]: Cursor's initial X position at the start of the drag
    //   param[1]: Cursor's initial Y position
    //   param[2]: X component of cursor's offset from upper-left corner of
    //             tab at start of drag
    //   param[3]: Y component of cursor's offset
    WINDOW_TYPE_CHROME_FLOATING_TAB,

    // The contents of a popup window.
    //   param[0]: X ID of associated titlebar, which must be mapped before
    //             its content
    //   param[1]: Initial state for panel (0 is collapsed, 1 is expanded)
    WINDOW_TYPE_CHROME_PANEL_CONTENT,

    // A small window representing a collapsed panel in the panel bar and
    // drawn above the panel when it's expanded.
    WINDOW_TYPE_CHROME_PANEL_TITLEBAR,

    // A small window that when clicked creates a new browser window.
    WINDOW_TYPE_CREATE_BROWSER_WINDOW,

    // A Chrome info bubble (e.g. the bookmark bubble).  These are
    // transient RGBA windows; we skip the usual transient behavior of
    // centering them over their owner and omit drawing a drop shadow.
    WINDOW_TYPE_CHROME_INFO_BUBBLE,

    // A window showing a view of a tab within a Chrome window.
    //   param[0]: X ID of toplevel window that owns it.
    //   param[1]: index of this tab in the tab order (range is 0 to
    //             sum of all tabs in all browsers).
    WINDOW_TYPE_CHROME_TAB_SNAPSHOT,

    // The following types are used for the windows that represent a user that
    // has already logged into the system.
    //
    // Visually the BORDER contains the IMAGE and CONTROLS windows, the LABEL
    // and UNSELECTED_LABEL are placed beneath the BORDER. The LABEL window is
    // onscreen when the user is selected, otherwise the UNSELECTED_LABEL is
    // on screen. The GUEST window is used when the user clicks on the entry
    // that represents the 'guest' user.
    //
    // The following parameters are set for these windows (except GUEST and
    // BACKGROUND):
    //   param[0]: the visual index of the user the window corresponds to.
    //             For example, all windows with an index of 0 occur first,
    //             followed by windows with an index of 1...
    //
    // The following additional params are set on the first BORDER window
    // (BORDER window whose param[0] == 0).
    //   param[1]: the total number of users.
    //   param[2]: size of the unselected image.
    //   param[3]: gap between image and controls.
    WINDOW_TYPE_LOGIN_BORDER,
    WINDOW_TYPE_LOGIN_IMAGE,
    WINDOW_TYPE_LOGIN_CONTROLS,
    WINDOW_TYPE_LOGIN_LABEL,
    WINDOW_TYPE_LOGIN_UNSELECTED_LABEL,
    WINDOW_TYPE_LOGIN_GUEST,
    WINDOW_TYPE_LOGIN_BACKGROUND,

    kNumWindowTypes,
  };

  struct Message {
   public:
    // NOTE: Don't remove values from this enum; it is shared between
    // Chrome and the window manager.
    enum Type {
      UNKNOWN = 0,

      // Notify Chrome when a floating tab has entered or left a tab
      // summary window.  Sent to the summary window.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered or currently in)
      //   param[2]: X coordinate relative to summary window
      //   param[3]: Y coordinate
      CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY,

      // Notify Chrome when a floating tab has entered or left a top-level
      // window.  Sent to the window being entered/left.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered)
      CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL,

      // Instruct a top-level Chrome window to change the visibility of its
      // tab summary window.
      //   param[0]: desired visibility (0 means hide, 1 means show)
      //   param[1]: X position (relative to the left edge of the root
      //             window) of the center of the top-level window.  Only
      //             relevant for "show" messages
      CHROME_SET_TAB_SUMMARY_VISIBILITY,

      // Tell the WM to collapse or expand a panel.
      //   param[0]: X ID of the panel window
      //   param[1]: desired state (0 means collapsed, 1 means expanded)
      WM_SET_PANEL_STATE,

      // Notify Chrome that the panel state has changed.  Sent to the panel
      // window.
      //   param[0]: new state (0 means collapsed, 1 means expanded)
      CHROME_NOTIFY_PANEL_STATE,

      // Instruct the WM to move a floating tab.  The passed-in position is
      // that of the cursor; the tab's composited window is displaced based
      // on the cursor's offset from the upper-left corner of the tab at
      // the start of the drag.
      //   param[0]: X ID of the floating tab window
      //   param[1]: X coordinate to which the tab should be moved
      //   param[2]: Y coordinate
      WM_MOVE_FLOATING_TAB,

      // Notify the WM that a panel has been dragged.
      //   param[0]: X ID of the panel's content window
      //   param[1]: X coordinate to which the upper-right corner of the
      //             panel's titlebar window was dragged
      //   param[2]: Y coordinate to which the upper-right corner of the
      //             panel's titlebar window was dragged
      // Note: The point given is actually that of one pixel to the right
      // of the upper-right corner of the titlebar window.  For example, a
      // no-op move message for a 10-pixel wide titlebar whose upper-left
      // point is at (0, 0) would contain the X and Y paremeters (10, 0):
      // in other words, the position of the titlebar's upper-left point
      // plus its width.  This is intended to make both the Chrome and WM
      // side of things simpler and to avoid some easy-to-make off-by-one
      // errors.
      WM_NOTIFY_PANEL_DRAGGED,

      // Notify the WM that the panel drag is complete (that is, the mouse
      // button has been released).
      //   param[0]: X ID of the panel's content window
      WM_NOTIFY_PANEL_DRAG_COMPLETE,

      // Deprecated.  Send a _NET_ACTIVE_WINDOW client message to focus a window
      // instead (e.g. using gtk_window_present()).
      DEPRECATED_WM_FOCUS_WINDOW,

      // Notify Chrome that the layout mode (for example, overview or
      // focused) has changed.
      //   param[0]: new mode (0 means focused, 1 means overview)
      CHROME_NOTIFY_LAYOUT_MODE,

      // Instruct the WM to enter overview mode.
      //   param[0]: X ID of the window to show the tab overview for.
      WM_SWITCH_TO_OVERVIEW_MODE,

      // Let the WM know which version of this file Chrome is using.  It's
      // difficult to make changes synchronously to Chrome and the WM (our
      // build scripts can use a locally-built Chromium, the latest one
      // from the buildbot, or an older hardcoded version), so it's useful
      // to be able to maintain compatibility in the WM with versions of
      // Chrome that exhibit older behavior.
      //
      // Chrome should send a message to the WM at startup containing the
      // latest version from the list below.  For backwards compatibility,
      // the WM assumes version 0 if it doesn't receive a message.  Here
      // are the changes that have been made in successive versions of the
      // protocol:
      //
      // 1: WM_NOTIFY_PANEL_DRAGGED contains the position of the
      //    upper-right, rather than upper-left, corner of of the titlebar
      //    window
      //
      // TODO: The latest version should be hardcoded in this file once the
      // file is being shared between Chrome and the WM so Chrome can just
      // pull it from there.  Better yet, the message could be sent
      // automatically in WmIpc's c'tor.
      //
      //   param[0]: version of this protocol currently supported
      WM_NOTIFY_IPC_VERSION,

      // Notify Chrome when a tab snapshot has been 'magnified' in the
      // overview.  Sent to the top level window.
      //   param[0]: X ID of the tab snapshot window
      //   param[1]: state (0 means end magnify, 1 means begin magnify)
      CHROME_NOTIFY_TAB_SNAPSHOT_MAGNIFY,

      // Forces the window manager to hide the login windows.
      WM_HIDE_LOGIN,

      // Sets whether login is enabled. If true the user can click on any of the
      // login windows to select one, if false clicks on unselected windows are
      // ignored. This is used when the user attempts a login to make sure the
      // user doesn't select another user.
      //
      //   param[0]: true to enable, false to disable.
      WM_SET_LOGIN_STATE,

      // Notify chrome when the guest entry is selected and the guest window
      // hasn't been created yet.
      CHROME_CREATE_GUEST_WINDOW,

      kNumTypes,
    };

    Message() {
      Init(UNKNOWN);
    }
    explicit Message(Type type) {
      Init(type);
    }

    Type type() const { return type_; }
    void set_type(Type type) { type_ = type; }

    inline int max_params() const {
      return arraysize(params_);
    }
    long param(int index) const {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      return params_[index];
    }
    void set_param(int index, long value) {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      params_[index] = value;
    }

   private:
    // Common initialization code shared between constructors.
    void Init(Type type) {
      set_type(type);
      for (int i = 0; i < max_params(); ++i) {
        set_param(i, 0);
      }
    }

    // Type of message that was sent.
    Type type_;

    // Type-specific data.  This is bounded by the number of 32-bit values
    // that we can pack into a ClientMessageEvent -- it holds five, but we
    // use the first one to store the message type.
    long params_[4];
  };

  // Returns the single instance of WmIpc.
  static WmIpc* instance();

  // Get or set a property describing a window's type.  Type-specific
  // parameters may also be supplied ('params' is mandatory for
  // GetWindowType() but optional for SetWindowType()).  The caller is
  // responsible for trapping errors from the X server.
  // TODO: Trap these ourselves.
  bool SetWindowType(GtkWidget* widget,
                     WindowType type,
                     const std::vector<int>* params);

  // Sends a message to the WM.
  void SendMessage(const Message& msg);

  // If |event| is a valid Message it is decoded into |msg| and true is
  // returned. If false is returned, |event| is not a valid Message.
  bool DecodeMessage(const GdkEventClient& event, Message* msg);

  // If |event| is a valid StringMessage it is decoded into |msg| and true is
  // returned. If false is returned, |event| is not a valid StringMessage.
  bool DecodeStringMessage(const GdkEventProperty& event, std::string* msg);

  // Handle ClientMessage events that weren't decodable using DecodeMessage().
  // Specifically, this catches messages about the WM_S0 selection that get sent
  // when a window manager process starts (so that we can re-run InitWmInfo()).
  // See ICCCM 2.8 for more info about MANAGER selections.
  void HandleNonChromeClientMessageEvent(const GdkEventClient& event);

 private:
  friend struct DefaultSingletonTraits<WmIpc>;

  WmIpc();

  // Initialize 'wm_' and send the window manager a message telling it the
  // version of the IPC protocol that we support.  This is called in our
  // constructor, but needs to be re-run if the window manager gets restarted.
  void InitWmInfo();

  // Maps from our Atom enum to the X server's atom IDs and from the
  // server's IDs to atoms' string names.  These maps aren't necessarily in
  // sync; 'atom_to_xatom_' is constant after the constructor finishes but
  // GetName() caches additional string mappings in 'xatom_to_string_'.
  std::map<AtomType, Atom> type_to_atom_;
  std::map<Atom, std::string> atom_to_string_;

  // Cached value of type_to_atom_[ATOM_CHROME_WM_MESSAGE].
  Atom wm_message_atom_;

  // Handle to the wm. Used for sending messages.
  XID wm_;

  DISALLOW_COPY_AND_ASSIGN(WmIpc);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_IPC_H_
