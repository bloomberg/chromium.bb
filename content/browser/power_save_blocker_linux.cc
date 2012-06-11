// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
// Xlib #defines Status, but we can't have that for some of our headers.
#ifdef Status
#undef Status
#endif

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop_proxy.h"
#if defined(TOOLKIT_GTK)
#include "base/message_pump_gtk.h"
#else
#include "base/message_pump_aurax11.h"
#endif
#include "base/nix/xdg_util.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace {

enum DBusAPI {
  NO_API,           // Disable. No supported API available.
  GNOME_API,        // Use the GNOME API. (Supports more features.)
  FREEDESKTOP_API,  // Use the FreeDesktop API, for KDE4 and XFCE.
};

// Inhibit flags defined in the org.gnome.SessionManager interface.
// Can be OR'd together and passed as argument to the Inhibit() method
// to specify which power management features we want to suspend.
enum GnomeAPIInhibitFlags {
  INHIBIT_LOGOUT            = 1,
  INHIBIT_SWITCH_USER       = 2,
  INHIBIT_SUSPEND_SESSION   = 4,
  INHIBIT_MARK_SESSION_IDLE = 8
};

const char kGnomeAPIServiceName[] = "org.gnome.SessionManager";
const char kGnomeAPIInterfaceName[] = "org.gnome.SessionManager";
const char kGnomeAPIObjectPath[] = "/org/gnome/SessionManager";

const char kFreeDesktopAPIServiceName[] = "org.freedesktop.PowerManagement";
const char kFreeDesktopAPIInterfaceName[] =
    "org.freedesktop.PowerManagement.Inhibit";
const char kFreeDesktopAPIObjectPath[] =
    "/org/freedesktop/PowerManagement/Inhibit";

}  // anonymous namespace

namespace content {

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  // Picks an appropriate D-Bus API to use based on the desktop environment.
  Delegate(PowerSaveBlockerType type, const std::string& reason);

  // Apply or remove the power save block, respectively. These methods should be
  // called once each, on the same thread, per instance. They block waiting for
  // the action to complete (with a timeout); the thread must thus allow IO.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}

  // If DPMS (the power saving system in X11) is not enabled, then we don't want
  // to try to disable power saving, since on some desktop environments that may
  // enable DPMS with very poor default settings (e.g. turning off the display
  // after only 1 second).
  static bool DPMSEnabled();

  // Returns an appropriate D-Bus API to use based on the desktop environment.
  static DBusAPI SelectAPI();

  const PowerSaveBlockerType type_;
  const std::string reason_;
  const DBusAPI api_;

  scoped_refptr<dbus::Bus> bus_;

  // The cookie that identifies our inhibit request,
  // or 0 if there is no active inhibit request.
  uint32 inhibit_cookie_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlocker::Delegate::Delegate(PowerSaveBlockerType type,
                                     const std::string& reason)
    : type_(type),
      reason_(reason),
      api_(SelectAPI()),
      inhibit_cookie_(0) {
  // We're on the client's thread here, so we don't allocate the dbus::Bus
  // object yet. We'll do it below in ApplyBlock(), on the FILE thread.
}

void PowerSaveBlocker::Delegate::ApplyBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!bus_.get());  // ApplyBlock() should only be called once.
  if (api_ == NO_API)
    return;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  bus_ = new dbus::Bus(options);

  scoped_refptr<dbus::ObjectProxy> object_proxy;
  scoped_ptr<dbus::MethodCall> method_call;
  scoped_ptr<dbus::MessageWriter> message_writer;

  switch (api_) {
    case NO_API:
      NOTREACHED();  // We return early above.
      return;
    case GNOME_API:
      object_proxy = bus_->GetObjectProxy(
          kGnomeAPIServiceName,
          dbus::ObjectPath(kGnomeAPIObjectPath));
      method_call.reset(
          new dbus::MethodCall(kGnomeAPIInterfaceName, "Inhibit"));
      message_writer.reset(new dbus::MessageWriter(method_call.get()));
      // The arguments of the method are:
      //     app_id:        The application identifier
      //     toplevel_xid:  The toplevel X window identifier
      //     reason:        The reason for the inhibit
      //     flags:         Flags that spefify what should be inhibited
      message_writer->AppendString(
          CommandLine::ForCurrentProcess()->GetProgram().value());
      message_writer->AppendUint32(0);  // should be toplevel_xid
      message_writer->AppendString(reason_);
      {
        uint32 flags = 0;
        switch (type_) {
          case kPowerSaveBlockPreventDisplaySleep:
            flags |= INHIBIT_MARK_SESSION_IDLE;
            flags |= INHIBIT_SUSPEND_SESSION;
            break;
          case kPowerSaveBlockPreventAppSuspension:
            flags |= INHIBIT_SUSPEND_SESSION;
            break;
        }
        message_writer->AppendUint32(flags);
      }
      break;
    case FREEDESKTOP_API:
      object_proxy = bus_->GetObjectProxy(
          kFreeDesktopAPIServiceName,
          dbus::ObjectPath(kFreeDesktopAPIObjectPath));
      method_call.reset(
          new dbus::MethodCall(kFreeDesktopAPIInterfaceName, "Inhibit"));
      message_writer.reset(new dbus::MessageWriter(method_call.get()));
      // The arguments of the method are:
      //     app_id:        The application identifier
      //     reason:        The reason for the inhibit
      message_writer->AppendString(
          CommandLine::ForCurrentProcess()->GetProgram().value());
      message_writer->AppendString(reason_);
      break;
  }

  // We could do this method call asynchronously, but if we did, we'd need to
  // handle the case where we want to cancel the block before we get a reply.
  // We're on the FILE thread so it should be OK to block briefly here.
  scoped_ptr<dbus::Response> response(object_proxy->CallMethodAndBlock(
      method_call.get(), dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (response.get()) {
    // The method returns an inhibit_cookie, used to uniquely identify
    // this request. It should be used as an argument to Uninhibit()
    // in order to remove the request.
    dbus::MessageReader message_reader(response.get());
    if (!message_reader.PopUint32(&inhibit_cookie_))
      LOG(ERROR) << "Invalid Inhibit() response: " << response->ToString();
  } else {
    LOG(ERROR) << "No response to Inhibit() request!";
  }
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (api_ == NO_API)
    return;
  DCHECK(bus_.get());  // RemoveBlock() should only be called once.

  scoped_refptr<dbus::ObjectProxy> object_proxy;
  scoped_ptr<dbus::MethodCall> method_call;

  switch (api_) {
    case NO_API:
      NOTREACHED();  // We return early above.
      return;
    case GNOME_API:
      object_proxy = bus_->GetObjectProxy(
          kGnomeAPIServiceName,
          dbus::ObjectPath(kGnomeAPIObjectPath));
      method_call.reset(
          new dbus::MethodCall(kGnomeAPIInterfaceName, "Uninhibit"));
      break;
    case FREEDESKTOP_API:
      object_proxy = bus_->GetObjectProxy(
          kFreeDesktopAPIServiceName,
          dbus::ObjectPath(kFreeDesktopAPIObjectPath));
      method_call.reset(
          new dbus::MethodCall(kFreeDesktopAPIInterfaceName, "Uninhibit"));
      break;
  }

  dbus::MessageWriter message_writer(method_call.get());
  message_writer.AppendUint32(inhibit_cookie_);
  scoped_ptr<dbus::Response> response(object_proxy->CallMethodAndBlock(
      method_call.get(), dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get())
    LOG(ERROR) << "No response to Uninhibit() request!";
  // We don't care about checking the result. We assume it works; we can't
  // really do anything about it anyway if it fails.
  inhibit_cookie_ = 0;

  bus_->ShutdownAndBlock();
  bus_ = NULL;
}

// static
bool PowerSaveBlocker::Delegate::DPMSEnabled() {
  Display* display = base::MessagePumpForUI::GetDefaultXDisplay();
  BOOL enabled = false;
  int dummy;
  if (DPMSQueryExtension(display, &dummy, &dummy) && DPMSCapable(display)) {
    CARD16 state;
    DPMSInfo(display, &state, &enabled);
  }
  return enabled;
}

// static
DBusAPI PowerSaveBlocker::Delegate::SelectAPI() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      if (DPMSEnabled())
        return GNOME_API;
      break;
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      if (DPMSEnabled())
        return FREEDESKTOP_API;
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      // Not supported.
      break;
  }
  return NO_API;
}

PowerSaveBlocker::PowerSaveBlocker(
    PowerSaveBlockerType type, const std::string& reason)
    : delegate_(new Delegate(type, reason)) {
  // The thread we use here becomes the origin and D-Bus thread for the D-Bus
  // library, so we need to use the same thread below in the destructor. It must
  // be a thread that allows I/O operations.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlocker::~PowerSaveBlocker() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
