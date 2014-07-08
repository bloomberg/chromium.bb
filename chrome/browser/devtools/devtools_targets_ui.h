// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/devtools/device/port_forwarding_controller.h"

namespace base {
class ListValue;
class DictionaryValue;
}

class DevToolsTargetImpl;
class Profile;

class DevToolsTargetsUIHandler {
 public:
  typedef base::Callback<void(const std::string&,
                              const base::ListValue&)> Callback;
  typedef base::Callback<void(DevToolsTargetImpl*)> TargetCallback;

  DevToolsTargetsUIHandler(const std::string& source_id,
                           const Callback& callback);
  virtual ~DevToolsTargetsUIHandler();

  std::string source_id() const { return source_id_; }

  static scoped_ptr<DevToolsTargetsUIHandler> CreateForRenderers(
      const Callback& callback);

  static scoped_ptr<DevToolsTargetsUIHandler> CreateForWorkers(
      const Callback& callback);

  static scoped_ptr<DevToolsTargetsUIHandler> CreateForAdb(
      const Callback& callback, Profile* profile);

  DevToolsTargetImpl* GetTarget(const std::string& target_id);

  virtual void Open(const std::string& browser_id, const std::string& url,
                    const TargetCallback& callback);

  virtual scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      const std::string& browser_id);

 protected:
  base::DictionaryValue* Serialize(const DevToolsTargetImpl& target);
  void SendSerializedTargets(const base::ListValue& list);

  typedef std::map<std::string, DevToolsTargetImpl*> TargetMap;
  TargetMap targets_;

 private:
  const std::string source_id_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTargetsUIHandler);
};

class PortForwardingStatusSerializer
    : private PortForwardingController::Listener {
 public:
  typedef base::Callback<void(const base::Value&)> Callback;

  PortForwardingStatusSerializer(const Callback& callback, Profile* profile);
  virtual ~PortForwardingStatusSerializer();

  virtual void PortStatusChanged(const DevicesStatus&) OVERRIDE;

 private:
  Callback callback_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
