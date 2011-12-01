// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop_proxy.h"
#include "base/nix/xdg_util.h"
#include "base/threading/platform_thread.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

using content::BrowserThread;

namespace {

// This class is used to inhibit Power Management on Linux systems
// using D-Bus interfaces. Mainly, there are two interfaces that
// make this possible.
// org.freedesktop.PowerManagement[.Inhibit] is considered to be
// the desktop-agnostic solution. However,  it is only used by
// KDE4 and XFCE.
// org.gnome.SessionManager is the Power Management interface
// available on Gnome desktops.
// Given that there is no generic solution to this problem,
// this class delegates the task of calling specific D-Bus APIs,
// to a DBusPowerSaveBlock::Delegate object.
// This class is a Singleton and the delegate will be instantiated
// internally, when the singleton instance is created, based on
// the desktop environment in which the application is running.
// When the class is instantiated, if it runs under a supported
// desktop environment it creates the Bus object and the
// delegate. Otherwise, no object is created and the ApplyBlock
// method will not do anything.
class DBusPowerSaveBlocker {
 public:
  // String passed to D-Bus APIs as the reason for which
  // the power management features are temporarily disabled.
  static const char kPowerSaveReason[];

  // This delegate interface represents a concrete
  // implementation for a specific D-Bus interface.
  // It is responsible for obtaining specific object proxies,
  // making D-Bus method calls and handling D-Bus responses.
  // When a new DBusPowerBlocker is created, only a specific
  // implementation of the delegate is instantiated. See the
  // DBusPowerSaveBlocker constructor for more details.
  // This is ref_counted to make sure that the callbacks
  // stay alive even after the DBusPowerSaveBlocker object
  // is deleted.
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void ApplyBlock(PowerSaveBlocker::PowerSaveBlockerType type) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns a pointer to the sole instance of this class
  static DBusPowerSaveBlocker* GetInstance();

  // Forwards a power save block request to the concrete implementation
  // of the Delegate interface.
  // If |delegate_| is NULL, the application runs under an unsupported
  // desktop environment. In this case, the method doesn't do anything.
  void ApplyBlock(PowerSaveBlocker::PowerSaveBlockerType type) {
    if (delegate_ != NULL) delegate_->ApplyBlock(type);
  }

  // Getter for the Bus object. Used by the Delegates to obtain object proxies.
  scoped_refptr<dbus::Bus> bus() const { return bus_; }

 private:
  DBusPowerSaveBlocker();
  virtual ~DBusPowerSaveBlocker();

  // The D-Bus connection.
  scoped_refptr<dbus::Bus> bus_;

  // Concrete implementation of the Delegate interface.
  scoped_refptr<Delegate> delegate_;

  friend struct DefaultSingletonTraits<DBusPowerSaveBlocker>;

  DISALLOW_COPY_AND_ASSIGN(DBusPowerSaveBlocker);
};

// Delegate implementation for KDE4.
// It uses the org.freedesktop.PowerManagement interface.
// Works on XFCE4, too.
class KDEPowerSaveBlocker: public DBusPowerSaveBlocker::Delegate {
 public:
  KDEPowerSaveBlocker()
      : inhibit_cookie_(0),
        pending_inhibit_call_(false),
        postponed_uninhibit_call_(false) {}
  ~KDEPowerSaveBlocker() {}

  virtual void ApplyBlock(PowerSaveBlocker::PowerSaveBlockerType type) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(pending_inhibit_call_ || !postponed_uninhibit_call_);

    // If we have a pending inhibit call, we add a postponed uninhibit
    // request, such that it will be canceled as soon as the response arrives.
    // If we have an active inhibit request and receive a new one,
    // we ignore it since the 'freedesktop' interface has only one Inhibit level
    // and we cannot differentiate between SystemSleep and DisplaySleep,
    // so there's no need to make additional D-Bus method calls.
    if (type == PowerSaveBlocker::kPowerSaveBlockPreventNone) {
      if (pending_inhibit_call_ && postponed_uninhibit_call_) {
        return;
      } else if (pending_inhibit_call_ && !postponed_uninhibit_call_) {
        postponed_uninhibit_call_ = true;
        return;
      } else if (!pending_inhibit_call_ && inhibit_cookie_ == 0) {
        return;
      }
    } else if ((pending_inhibit_call_ && !postponed_uninhibit_call_) ||
                inhibit_cookie_ > 0) {
        return;
    }

    scoped_refptr<dbus::ObjectProxy> object_proxy =
        DBusPowerSaveBlocker::GetInstance()->bus()->GetObjectProxy(
            "org.freedesktop.PowerManagement",
            "/org/freedesktop/PowerManagement/Inhibit");
    dbus::MethodCall method_call("org.freedesktop.PowerManagement.Inhibit",
                                 "Inhibit");
    dbus::MessageWriter message_writer(&method_call);
    base::Callback<void(dbus::Response*)> bus_callback;

    switch (type) {
      case PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
      case PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep:
        // The org.freedesktop.PowerManagement.Inhibit interface offers only one
        // Inhibit() method, that temporarily disables all power management
        // features. We cannot differentiate and disable individual features,
        // like display sleep or system sleep.
        // The first argument of the Inhibit method is the application name.
        // The second argument of the Inhibit method is a string containing
        // the reason of the power save block request.
        // The method returns a cookie (an int), which we must pass back to the
        // UnInhibit method when we cancel our request.
        message_writer.AppendString(base::PlatformThread::GetName());
        message_writer.AppendString(DBusPowerSaveBlocker::kPowerSaveReason);
        bus_callback = base::Bind(&KDEPowerSaveBlocker::OnInhibitResponse,
                                  this);
        pending_inhibit_call_ = true;
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventNone:
        // To cancel our inhibit request, we have to call a different method.
        // It takes one argument, the cookie returned by the corresponding
        // Inhibit method call.
        method_call.SetMember("UnInhibit");
        message_writer.AppendUint32(inhibit_cookie_);
        bus_callback = base::Bind(&KDEPowerSaveBlocker::OnUnInhibitResponse,
                                  this);
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventStateCount:
        // This is an invalid argument
        NOTREACHED();
        break;
    }

    object_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             bus_callback);
  }

 private:
  // Inhibit() response callback.
  // Stores the cookie so we can use it later when calling UnInhibit().
  // If the response from D-Bus is successful and there is a postponed
  // uninhibit request, we cancel the cookie that we just received.
  void OnInhibitResponse(dbus::Response* response) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(pending_inhibit_call_);
    pending_inhibit_call_ = false;
    if (response != NULL) {
      dbus::MessageReader message_reader(response);
      if (message_reader.PopUint32(&inhibit_cookie_)) {
        if (postponed_uninhibit_call_) {
          postponed_uninhibit_call_ = false;
          ApplyBlock(PowerSaveBlocker::kPowerSaveBlockPreventNone);
        }
        return;
      } else {
        LOG(ERROR) << "Invalid Inhibit() response: " << response->ToString();
      }
    }
    inhibit_cookie_ = 0;
    postponed_uninhibit_call_ = false;
  }

  // UnInhibit() method callback.
  // We set the |inhibit_cookie_| to 0 even if the D-Bus call failed.
  void OnUnInhibitResponse(dbus::Response* response) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    inhibit_cookie_ = 0;
  };

  // The cookie that identifies our last inhibit request,
  // or 0 if there is no active inhibit request.
  uint32 inhibit_cookie_;

  // True if we made an inhibit call for which
  // we did not receive a response yet
  bool pending_inhibit_call_;

  // True if we have to cancel the cookie we are about to receive
  bool postponed_uninhibit_call_;

  DISALLOW_COPY_AND_ASSIGN(KDEPowerSaveBlocker);
};

// Delegate implementation for Gnome, based on org.gnome.SessionManager
class GnomePowerSaveBlocker: public DBusPowerSaveBlocker::Delegate {
 public:
  // Inhibit flags defined in the org.gnome.SessionManager interface.
  // Can be OR'd together and passed as argument to the Inhibit() method
  // to specify which power management features we want to suspend.
  enum InhibitFlags {
    kInhibitLogOut            = 1,
    kInhibitSwitchUser        = 2,
    kInhibitSuspendSession    = 4,
    kInhibitMarkSessionAsIdle = 8
  };

  GnomePowerSaveBlocker()
      : inhibit_cookie_(0),
        pending_inhibit_calls_(0),
        postponed_uninhibit_calls_(0) {}
  ~GnomePowerSaveBlocker() {}

  virtual void ApplyBlock(PowerSaveBlocker::PowerSaveBlockerType type) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(postponed_uninhibit_calls_ <= pending_inhibit_calls_);

    // If we have a pending inhibit call, we add a postponed uninhibit
    // request, such that it will be canceled as soon as the response arrives.
    // We want to cancel the current inhibit request whether |type| is
    // kPowerSaveBlockPreventNone or not. If |type| represents an inhibit
    // request, we are dealing with the same case as below, just that the
    // reply to the previous inhibit request did not arrive yet, so we have
    // to wait for the cookie in order to cancel it.
    // Meanwhile, we can still make the new request.
    // We also have to check that
    // postponed_uninhibit_calls_ < pending_inhibit_calls_.
    // If this is not the case, then all the pending requests were already
    // canceled and we should not increment the number of postponed uninhibit
    // requests; otherwise we will cancel unwanted future inhibits,
    // that will be made after this call.
    // NOTE: The implementation is based on the fact that we receive
    // the D-Bus replies in the same order in which the requests are made.
    if (pending_inhibit_calls_ > 0 &&
        postponed_uninhibit_calls_ < pending_inhibit_calls_) {
      ++postponed_uninhibit_calls_;
      // If the call was an Uninhibit, then we are done for the moment.
      if (type == PowerSaveBlocker::kPowerSaveBlockPreventNone) return;
    }

    // If we have an active inhibit request and no pending inhibit calls,
    // we make an uninhibit request to cancel it now.
    if (type != PowerSaveBlocker::kPowerSaveBlockPreventNone &&
        pending_inhibit_calls_ == 0 &&
        inhibit_cookie_ > 0) {
      ApplyBlock(PowerSaveBlocker::kPowerSaveBlockPreventNone);
    }

    scoped_refptr<dbus::ObjectProxy> object_proxy =
        DBusPowerSaveBlocker::GetInstance()->bus()->GetObjectProxy(
            "org.gnome.SessionManager",
            "/org/gnome/SessionManager");
    dbus::MethodCall method_call("org.gnome.SessionManager", "Inhibit");
    dbus::MessageWriter message_writer(&method_call);
    base::Callback<void(dbus::Response*)> bus_callback;

    unsigned int flags = 0;
    switch (type) {
      case PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
        flags |= kInhibitMarkSessionAsIdle;
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep:
        flags |= kInhibitMarkSessionAsIdle;
        flags |= kInhibitSuspendSession;
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventNone:
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventStateCount:
        // This is an invalid argument
        NOTREACHED();
        break;
    }

    switch (type) {
      case PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
      case PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep:
        // To temporarily suspend the power management features on Gnome,
        // we call org.gnome.SessionManager.Inhibit().
        // The arguments of the method are:
        //     app_id:        The application identifier
        //     toplevel_xid:  The toplevel X window identifier
        //     reason:        The reason for the inhibit
        //     flags:         Flags that spefify what should be inhibited
        // The method returns and inhibit_cookie, used to uniquely identify
        // this request. It should be used as an argument to Uninhibit()
        // in order to remove the request.
        message_writer.AppendString(base::PlatformThread::GetName());
        message_writer.AppendUint32(0);  // should be toplevel_xid
        message_writer.AppendString(DBusPowerSaveBlocker::kPowerSaveReason);
        message_writer.AppendUint32(flags);
        bus_callback = base::Bind(&GnomePowerSaveBlocker::OnInhibitResponse,
                                  this);
        ++pending_inhibit_calls_;
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventNone:
        // To cancel a previous inhibit request we call
        // org.gnome.SessionManager.Uninhibit().
        // It takes only one argument, the cookie that identifies
        // the request we want to cancel.
        method_call.SetMember("Uninhibit");
        message_writer.AppendUint32(inhibit_cookie_);
        bus_callback = base::Bind(&GnomePowerSaveBlocker::OnUnInhibitResponse,
                                  this);
        ++pending_inhibit_calls_;
        break;
      case PowerSaveBlocker::kPowerSaveBlockPreventStateCount:
        // This is an invalid argument;
        NOTREACHED();
        break;
    }

    object_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             bus_callback);
  }

 private:
  // Inhibit() response callback.
  // Stores the cookie so we can use it later when calling UnInhibit().
  // If the response from D-Bus is successful and there is a postponed
  // uninhibit request, we cancel the cookie that we just received.
  void OnInhibitResponse(dbus::Response* response) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_GT(pending_inhibit_calls_, 0);
    --pending_inhibit_calls_;
    if (response != NULL) {
      dbus::MessageReader message_reader(response);
      if (message_reader.PopUint32(&inhibit_cookie_)) {
        if (postponed_uninhibit_calls_ > 0) {
          --postponed_uninhibit_calls_;
          ApplyBlock(PowerSaveBlocker::kPowerSaveBlockPreventNone);
        }
        return;
      } else {
        LOG(ERROR) << "Invalid Inhibit() response: " << response->ToString();
      }
    }
    inhibit_cookie_ = 0;
    if (postponed_uninhibit_calls_ > 0) {
      --postponed_uninhibit_calls_;
    }
  }

  // Uninhibit() response callback.
  // We set the |inhibit_cookie_| to 0 even if the D-Bus call failed.
  void OnUnInhibitResponse(dbus::Response* response) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    inhibit_cookie_ = 0;
  };

  // The cookie that identifies our last inhibit request,
  // or 0 if there is no active inhibit request.
  uint32 inhibit_cookie_;

  // Store the number of inhibit calls for which
  // we did not receive a response yet
  int pending_inhibit_calls_;

  // Store the number of Uninhibit requests that arrived,
  // before the corresponding Inhibit calls were completed.
  int postponed_uninhibit_calls_;

  DISALLOW_COPY_AND_ASSIGN(GnomePowerSaveBlocker);
};

const char DBusPowerSaveBlocker::kPowerSaveReason[] = "Power Save Blocker";

// Initialize the DBusPowerSaveBlocker instance:
// 1. Instantiate a concrete delegate based on the current desktop environment,
// 2. Instantiate the D-Bus object
DBusPowerSaveBlocker::DBusPowerSaveBlocker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<base::Environment> env(base::Environment::Create());
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      delegate_ = new GnomePowerSaveBlocker();
      break;
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      delegate_ = new KDEPowerSaveBlocker();
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      // Not supported, so we exit.
      // We don't create D-Bus objects.
      break;
  }

  if (delegate_ != NULL) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    // Use the FILE thread to service the D-Bus connection,
    // since we need a thread that allows I/O operations.
    options.dbus_thread_message_loop_proxy =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
    bus_ = new dbus::Bus(options);
  }
}

DBusPowerSaveBlocker::~DBusPowerSaveBlocker() {
  // We try to shut down the bus, but unfortunately in most of the
  // cases when we delete the singleton instance,
  // the FILE thread is already stopped and there is no way to
  // shutdown the bus object on the origin thread (the UI thread).
  // However, this is not a crucial problem since at this point
  // we are at the very end of the shutting down phase.
  // Connection to D-Bus is just a Unix domain socket, which is not
  // a persistent resource, hence the operating system will take care
  // of closing it when the process terminates.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    bus_->ShutdownOnDBusThreadAndBlock();
  }
}

// static
DBusPowerSaveBlocker* DBusPowerSaveBlocker::GetInstance() {
  return Singleton<DBusPowerSaveBlocker>::get();
}

}  // namespace

// Called only from UI thread.
// static
void PowerSaveBlocker::ApplyBlock(PowerSaveBlockerType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DBusPowerSaveBlocker::GetInstance()->ApplyBlock(type);
}
