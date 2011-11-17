// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"

#include <gdk/gdkx.h>
extern "C" {
#include <X11/Xatom.h>
#include <X11/Xlib.h>
}

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/rect.h"

using std::map;
using std::string;

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
  { WmIpc::ATOM_CHROME_LAYOUT_MODE,           "_CHROME_LAYOUT_MODE" },
  { WmIpc::ATOM_CHROME_LOGGED_IN,             "_CHROME_LOGGED_IN" },
  { WmIpc::ATOM_CHROME_STATE,                 "_CHROME_STATE" },
  { WmIpc::ATOM_CHROME_STATE_COLLAPSED_PANEL, "_CHROME_STATE_COLLAPSED_PANEL" },
  { WmIpc::ATOM_CHROME_STATE_STATUS_HIDDEN,   "_CHROME_STATE_STATUS_HIDDEN" },
  { WmIpc::ATOM_CHROME_STATUS_BOUNDS,         "_CHROME_STATUS_BOUNDS" },
  { WmIpc::ATOM_CHROME_WINDOW_TYPE,           "_CHROME_WINDOW_TYPE" },
  { WmIpc::ATOM_CHROME_WM_MESSAGE,            "_CHROME_WM_MESSAGE" },
  { WmIpc::ATOM_MANAGER,                      "MANAGER" },
  { WmIpc::ATOM_STRING,                       "STRING" },
  { WmIpc::ATOM_UTF8_STRING,                  "UTF8_STRING" },
  { WmIpc::ATOM_WM_S0,                        "WM_S0" },
};

bool SetIntArrayProperty(XID xid,
                         Atom property,
                         Atom property_type,
                         const std::vector<int>& values) {
  DCHECK(!values.empty());

  // XChangeProperty expects values of type 32 to be longs.
  scoped_array<long> data(new long[values.size()]);
  for (size_t i = 0; i < values.size(); ++i)
    data[i] = values[i];

  // TODO: Trap errors and return false on failure.
  XChangeProperty(ui::GetXDisplay(),
                  xid,
                  property,
                  property_type,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  values.size());  // num items
  XFlush(ui::GetXDisplay());
  return true;
}

}  // namespace

static base::LazyInstance<WmIpc> g_wm_ipc = LAZY_INSTANCE_INITIALIZER;

// static
WmIpc* WmIpc::instance() {
  return g_wm_ipc.Pointer();
}

string WmIpc::GetAtomName(AtomType type) const {
  map<AtomType, Atom>::const_iterator atom_it = type_to_atom_.find(type);
  DCHECK(atom_it != type_to_atom_.end()) << "Unknown AtomType " << type;
  if (atom_it == type_to_atom_.end())
    return "";

  map<Atom, string>::const_iterator name_it =
      atom_to_string_.find(atom_it->second);
  DCHECK(name_it != atom_to_string_.end())
      << "Unknown atom " << atom_it->second << " for AtomType " << type;
  if (name_it == atom_to_string_.end())
    return "";

  return name_it->second;
}

bool WmIpc::SetWindowType(GtkWidget* widget,
                          WmIpcWindowType type,
                          const std::vector<int>* params) {
  std::vector<int> values;
  values.push_back(type);
  if (params)
    values.insert(values.end(), params->begin(), params->end());
  const Atom atom = type_to_atom_[ATOM_CHROME_WINDOW_TYPE];
  return SetIntArrayProperty(ui::GetX11WindowFromGtkWidget(widget),
                             atom, atom, values);
}

WmIpcWindowType WmIpc::GetWindowType(GtkWidget* widget,
                                     std::vector<int>* params) {
  std::vector<int> properties;
  if (ui::GetIntArrayProperty(
          ui::GetX11WindowFromGtkWidget(widget),
          GetAtomName(ATOM_CHROME_WINDOW_TYPE),
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

bool WmIpc::GetWindowState(GtkWidget* widget,
                           std::set<WmIpc::AtomType>* atom_types) {
  atom_types->clear();

  std::vector<Atom> atoms;
  if (!ui::GetAtomArrayProperty(
          ui::GetX11WindowFromGtkWidget(widget),
          GetAtomName(ATOM_CHROME_STATE),
          &atoms)) {
    return false;
  }

  for (std::vector<Atom>::const_iterator it = atoms.begin();
       it != atoms.end(); ++it) {
    std::map<Atom, AtomType>::const_iterator type_it = atom_to_type_.find(*it);
    if (type_it != atom_to_type_.end())
      atom_types->insert(type_it->second);
    else
      LOG(WARNING) << "Unknown state atom " << *it;
  }
  return true;
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

void WmIpc::HandleRootWindowPropertyEvent(const GdkEventProperty& event) {
  static GdkAtom layout_mode_gdk_atom =
      gdk_x11_xatom_to_atom(type_to_atom_[ATOM_CHROME_LAYOUT_MODE]);

  if (event.atom == layout_mode_gdk_atom)
    FetchLayoutModeProperty();
}

void WmIpc::SetLoggedInProperty(bool logged_in) {
  std::vector<int> values;
  values.push_back(static_cast<int>(logged_in));
  const Atom atom = type_to_atom_[ATOM_CHROME_LOGGED_IN];
  SetIntArrayProperty(gdk_x11_get_default_root_xwindow(), atom, atom, values);
}

void WmIpc::SetStatusBoundsProperty(GtkWidget* widget,
                                    const gfx::Rect& bounds) {
  std::vector<int> values;
  values.push_back(bounds.x());
  values.push_back(bounds.y());
  values.push_back(bounds.width());
  values.push_back(bounds.height());
  SetIntArrayProperty(ui::GetX11WindowFromGtkWidget(widget),
                      type_to_atom_[ATOM_CHROME_STATUS_BOUNDS],
                      XA_CARDINAL,
                      values);
}

void WmIpc::NotifyAboutSignout() {
  Message msg(chromeos::WM_IPC_MESSAGE_WM_NOTIFY_SIGNING_OUT);
  SendMessage(msg);
  XFlush(ui::GetXDisplay());
}

WmIpc::WmIpc()
    : wm_message_atom_(0),
      wm_(0),
      layout_mode_(WM_IPC_LAYOUT_MAXIMIZED) {
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
    atom_to_type_[atoms[i]] = kAtomInfos[i].atom;
    atom_to_string_[atoms[i]] = kAtomInfos[i].name;
  }

  wm_message_atom_ = type_to_atom_[ATOM_CHROME_WM_MESSAGE];

  // Make sure that we're selecting structure changes on the root window;
  // the window manager uses StructureNotifyMask when sending the ClientMessage
  // event to announce that it's taken the manager selection.
  GdkWindow* root = gdk_get_default_root_window();
  GdkEventMask event_mask = gdk_window_get_events(root);
  gdk_window_set_events(
      root,
      static_cast<GdkEventMask>(
          event_mask | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK));

  InitWmInfo();
  FetchLayoutModeProperty();
}

WmIpc::~WmIpc() {}

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

void WmIpc::FetchLayoutModeProperty() {
  int value = 0;
  if (ui::GetIntProperty(
          gdk_x11_get_default_root_xwindow(),
          GetAtomName(ATOM_CHROME_LAYOUT_MODE),
          &value)) {
    layout_mode_ = static_cast<WmIpcLayoutMode>(value);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LAYOUT_MODE_CHANGED,
        content::Source<WmIpc>(this),
        content::Details<WmIpcLayoutMode>(&layout_mode_));
  } else {
    DLOG(WARNING) << "Missing _CHROME_LAYOUT_MODE property on root window";
  }
}

}  // namespace chromeos
