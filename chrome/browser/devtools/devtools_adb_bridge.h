// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/socket/tcp_client_socket.h"

namespace base {
class Thread;
class DictionaryValue;
}

class MessageLoop;
class Profile;

class DevToolsAdbBridge {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;

  class RemotePage : public base::RefCounted<RemotePage> {
   public:
    RemotePage(const std::string& serial,
               const std::string& model,
               const base::DictionaryValue& value);

    std::string serial() { return serial_; }
    std::string model() { return model_; }
    std::string id() { return id_; }
    std::string url() { return url_; }
    std::string title() { return title_; }
    std::string description() { return description_; }
    std::string favicon_url() { return favicon_url_; }
    std::string debug_url() { return debug_url_; }
    std::string frontend_url() { return frontend_url_; }

   private:
    friend class base::RefCounted<RemotePage>;
    virtual ~RemotePage();
    std::string serial_;
    std::string model_;
    std::string id_;
    std::string url_;
    std::string title_;
    std::string description_;
    std::string favicon_url_;
    std::string debug_url_;
    std::string frontend_url_;
    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  typedef std::vector<scoped_refptr<RemotePage> > RemotePages;
  typedef base::Callback<void(int, RemotePages*)> PagesCallback;

  explicit DevToolsAdbBridge(Profile* profile);
  ~DevToolsAdbBridge();

  void Query(const std::string query, const Callback& callback);
  void Pages(const PagesCallback& callback);
  void Attach(const std::string& serial,
              const std::string& debug_url,
              const std::string& frontend_url);

 private:
  friend class AdbAttachCommand;
  friend class AgentHostDelegate;

  class RefCountedAdbThread : public base::RefCounted<RefCountedAdbThread> {
   public:
    static scoped_refptr<RefCountedAdbThread> GetInstance();
    RefCountedAdbThread();
    MessageLoop* message_loop();

   private:
    friend class base::RefCounted<RefCountedAdbThread>;
    static DevToolsAdbBridge::RefCountedAdbThread* instance_;
    static void StopThread(base::Thread* thread);

    virtual ~RefCountedAdbThread();
    base::Thread* thread_;
  };

  Profile* profile_;
  scoped_refptr<RefCountedAdbThread> adb_thread_;
  base::WeakPtrFactory<DevToolsAdbBridge> weak_factory_;
  bool has_message_loop_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
