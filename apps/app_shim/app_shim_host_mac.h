// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MAC_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

class Profile;

namespace IPC {
struct ChannelHandle;
class ChannelProxy;
class Message;
}  // namespace IPC

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost owns itself, and is
// destroyed when the app it corresponds to is closed or when the channel
// connected to the app shim is closed.
class AppShimHost : public IPC::Listener,
                    public IPC::Sender,
                    public content::NotificationObserver,
                    public base::NonThreadSafe {
 public:
  AppShimHost();
  virtual ~AppShimHost();

  // Creates a new server-side IPC channel at |handle|, which should contain a
  // file descriptor of a channel created by an IPC::ChannelFactory, and begins
  // listening for messages on it.
  void ServeChannel(const IPC::ChannelHandle& handle);

  const std::string& app_id() const { return app_id_; }
  const Profile* profile() const { return profile_; }

 protected:
  // Used internally; virtual so they can be mocked for testing.
  virtual Profile* FetchProfileForDirectory(const std::string& profile_dir);
  virtual bool LaunchApp(Profile* profile, const std::string& app_id);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

 private:
  // The app shim process is requesting that an app be launched. Once it has
  // done so the |profile| and |app_id| are stored, and all future messages
  // from the app shim relate to the app it launched. It is an error for the
  // app shim to send multiple launch messages.
  void OnLaunchApp(std::string profile, std::string app_id);

  // Called when the app shim process notifies that the app should be brought
  // to the front (i.e. the user has clicked on the app's icon in the dock or
  // Cmd+Tabbed to it.)
  void OnFocus();

  bool LaunchAppImpl(const std::string& profile_dir, const std::string& app_id);

  // The AppShimHost listens to the NOTIFICATION_EXTENSION_HOST_DESTROYED
  // message to detect when the app closes. When that happens, the AppShimHost
  // closes the channel, which causes the app shim process to quit.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Closes the channel and destroys the AppShimHost.
  void Close();

  scoped_ptr<IPC::ChannelProxy> channel_;
  std::string app_id_;
  Profile* profile_;
  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MAC_H_
