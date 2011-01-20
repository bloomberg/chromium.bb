// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_ipc.h"

#include <gdk/gdkx.h>
extern "C" {
#include <X11/Xlib.h>
}

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "ui/base/x/x11_util.h"

namespace chromeos {

namespace {

// A value from the Atom enum and the actual name that should be used to
// look up its ID on the X server.
struct AtomInfo {
  WmIpc::AtomType atom;
  const char* name;
};

// Each value from the Atom enum must be present here.
static const AtomInfo kAtomInfos[] = {
  { WmIpc::ATOM_CHROME_LOGGED_IN,        "_CHROME_LOGGED_IN" },
  { WmIpc::ATOM_CHROME_WINDOW_TYPE,      "_CHROME_WINDOW_TYPE" },
  { WmIpc::ATOM_CHROME_WM_MESSAGE,       "_CHROME_WM_MESSAGE" },
  { WmIpc::ATOM_MANAGER,                 "MANAGER" },
  { WmIpc::ATOM_STRING,                  "STRING" },
  { WmIpc::ATOM_UTF8_STRING,             "UTF8_STRING" },
  { WmIpc::ATOM_WM_S0,                   "WM_S0" },
};

bool SetIntProperty(XID xid, Atom xatom, const std::vector<int>& values) {
  DCHECK(!values.empty());

  // XChangeProperty expects values of type 32 to be longs.
  scoped_array<long> data(new long[values.size()]);
  for (size_t i = 0; i < values.size(); ++i)
    data[i] = values[i];

  // TODO: Trap errors and return false on failure.
  XChangeProperty(ui::GetXDisplay(),
                  xid,
                  xatom,
                  xatom,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  values.size());  // num items
  XFlush(ui::GetXDisplay());
  return true;
}

}  // namespace

static base::LazyInstance<WmIpc> g_wm_ipc(base::LINKER_INITIALIZED);

// static
WmIpc* WmIpc::instance() {
  return g_wm_ipc.Pointer();
}

bool WmIpc::SetWindowType(GtkWidget* widget,
                          WmIpcWindowType type,
                          const std::vector<int>* params) {
  std::vector<int> values;
  values.push_back(type);
  if (params)
    values.insert(values.end(), params->begin(), params->end());
  return SetIntProperty(ui::GetX11WindowFromGtkWidget(widget),
                        type_to_atom_[ATOM_CHROME_WINDOW_TYPE], values);
}

WmIpcWindowType WmIpc::GetWindowType(GtkWidget* widget,
                                     std::vector<int>* params) {
  std::vector<int> properties;
  if (ui::GetIntArrayProperty(
          ui::GetX11WindowFromGtkWidget(widget),
          atom_to_string_[type_to_atom_[ATOM_CHROME_WINDOW_TYPE]],
          &properties)) {
    int type = properties.front();
    if (params) {
      params->clear();
      params->insert(params->begin(), properties.begin() + 1, properties.end());
    }
    return static_cast<WmIpcWindowType>(type);
  } else {
    return WM_IPC_WINDOW_UNKNOWN;
  }
}

void WmIpc::SendMessage(const Message& msg) {
  XEvent e;
  e.xclient.type = ClientMessage;
  e.xclient.window = wm_;
  e.xclient.message_type = type_to_atom_[ATOM_CHROME_WM_MESSAGE];
  e.xclient.format = 32;  // 32-bit values
  e.xclient.data.l[0] = msg.type();

  // XClientMessageEvent only gives us five 32-bit items, and we're using
  // the first one for our message type.
  DCHECK_LE(msg.max_params(), 4);
  for (int i = 0; i < msg.max_params(); ++i)
    e.xclient.data.l[i+1] = msg.param(i);

  XSendEvent(ui::GetXDisplay(),
             wm_,
             False,  // propagate
             0,  // empty event mask
             &e);
}

bool WmIpc::DecodeMessage(const GdkEventClient& event, Message* msg) {
  if (wm_message_atom_ != gdk_x11_atom_to_xatom(event.message_type))
    return false;

  if (event.data_format != 32) {
    DLOG(WARNING) << "Ignoring ClientEventMessage with invalid bit "
                  << "format " << event.data_format
                  << " (expected 32-bit values)";
    return false;
  }

  msg->set_type(static_cast<WmIpcMessageType>(event.data.l[0]));
  if (msg->type() < 0) {
    DLOG(WARNING) << "Ignoring ClientEventMessage with invalid message "
                  << "type " << msg->type();
    return false;
  }

  // XClientMessageEvent only gives us five 32-bit items, and we're using
  // the first one for our message type.
  DCHECK_LE(msg->max_params(), 4);
  for (int i = 0; i < msg->max_params(); ++i)
    msg->set_param(i, event.data.l[i+1]);  // l[0] contains message type

  return true;
}

void WmIpc::HandleNonChromeClientMessageEvent(const GdkEventClient& event) {
  // Only do these lookups once; they should never change.
  static GdkAtom manager_gdk_atom =
      gdk_x11_xatom_to_atom(type_to_atom_[ATOM_MANAGER]);
  static Atom wm_s0_atom = type_to_atom_[ATOM_WM_S0];

  if (event.message_type == manager_gdk_atom &&
      static_cast<Atom>(event.data.l[1]) == wm_s0_atom) {
    InitWmInfo();
  }
}

void WmIpc::SetLoggedInProperty(bool logged_in) {
  std::vector<int> values;
  values.push_back(static_cast<int>(logged_in));
  SetIntProperty(gdk_x11_get_default_root_xwindow(),
                 type_to_atom_[ATOM_CHROME_LOGGED_IN],
                 values);
}

void WmIpc::NotifyAboutSignout() {
  Message msg(chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SIGNING_OUT);
  SendMessage(msg);
  XFlush(ui::GetXDisplay());
}

WmIpc::WmIpc() {
  scoped_array<char*> names(new char*[kNumAtoms]);
  scoped_array<Atom> atoms(new Atom[kNumAtoms]);

  for (int i = 0; i < kNumAtoms; ++i) {
    // Need to const_cast because XInternAtoms() wants a char**.
    names[i] = const_cast<char*>(kAtomInfos[i].name);
  }

  XInternAtoms(ui::GetXDisplay(), names.get(), kNumAtoms,
               False,  // only_if_exists
               atoms.get());

  for (int i = 0; i < kNumAtoms; ++i) {
    type_to_atom_[kAtomInfos[i].atom] = atoms[i];
    atom_to_string_[atoms[i]] = kAtomInfos[i].name;
  }

  wm_message_atom_ = type_to_atom_[ATOM_CHROME_WM_MESSAGE];

  // Make sure that we're selecting structure changes on the root window;
  // the window manager uses StructureNotifyMask when sending the ClientMessage
  // event to announce that it's taken the manager selection.
  GdkWindow* root = gdk_get_default_root_window();
  GdkEventMask event_mask = gdk_window_get_events(root);
  gdk_window_set_events(
      root, static_cast<GdkEventMask>(event_mask | GDK_STRUCTURE_MASK));

  InitWmInfo();
}

void WmIpc::InitWmInfo() {
  wm_ = XGetSelectionOwner(ui::GetXDisplay(), type_to_atom_[ATOM_WM_S0]);

  // Let the window manager know which version of the IPC messages we support.
  Message msg(chromeos::WM_IPC_MESSAGE_WM_NOTIFY_IPC_VERSION);
  // TODO: The version number is the latest listed in wm_ipc.h --
  // ideally, once this header is shared between Chrome and the Chrome OS window
  // manager, we'll just define the version statically in the header.
  msg.set_param(0, 1);
  SendMessage(msg);
}

}  // namespace chromeos
