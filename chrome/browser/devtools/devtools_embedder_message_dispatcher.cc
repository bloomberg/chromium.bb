// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include "base/bind.h"
#include "base/values.h"

namespace {

using DispatchCallback = DevToolsEmbedderMessageDispatcher::DispatchCallback;

bool GetValue(const base::Value* value, std::string* result) {
  return value->GetAsString(result);
}

bool GetValue(const base::Value* value, int* result) {
  return value->GetAsInteger(result);
}

bool GetValue(const base::Value* value, bool* result) {
  return value->GetAsBoolean(result);
}

bool GetValue(const base::Value* value, gfx::Rect* rect) {
  const base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict))
    return false;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  if (!dict->GetInteger("x", &x) ||
      !dict->GetInteger("y", &y) ||
      !dict->GetInteger("width", &width) ||
      !dict->GetInteger("height", &height))
    return false;
  rect->SetRect(x, y, width, height);
  return true;
}

template <typename T>
struct StorageTraits {
  using StorageType = T;
};

template <typename T>
struct StorageTraits<const T&> {
  using StorageType = T;
};

// TODO(dgozman): move back to variadic templates once it compiles everywhere.
// See http://crbug.com/491973.

bool ParseAndHandle0(const base::Callback<void()>& handler,
                     const DispatchCallback& callback,
                     const base::ListValue& list) {
  if (list.GetSize() != 0)
    return false;
  handler.Run();
  return true;
}

template<class A1>
bool ParseAndHandle1(const base::Callback<void(A1)>& handler,
                     const DispatchCallback& callback,
                     const base::ListValue& list) {
  if (list.GetSize() != 1)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  handler.Run(a1);
  return true;
}

template<class A1, class A2>
bool ParseAndHandle2(const base::Callback<void(A1, A2)>& handler,
                     const DispatchCallback& callback,
                     const base::ListValue& list) {
  if (list.GetSize() != 2)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  const base::Value* value2;
  list.Get(1, &value2);
  typename StorageTraits<A2>::StorageType a2;
  if (!GetValue(value2, &a2))
    return false;
  handler.Run(a1, a2);
  return true;
}

template<class A1, class A2, class A3>
bool ParseAndHandle3(const base::Callback<void(A1, A2, A3)>& handler,
                     const DispatchCallback& callback,
                     const base::ListValue& list) {
  if (list.GetSize() != 3)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  const base::Value* value2;
  list.Get(1, &value2);
  typename StorageTraits<A2>::StorageType a2;
  if (!GetValue(value2, &a2))
    return false;
  const base::Value* value3;
  list.Get(2, &value3);
  typename StorageTraits<A3>::StorageType a3;
  if (!GetValue(value3, &a3))
    return false;
  handler.Run(a1, a2, a3);
  return true;
}

bool ParseAndHandleWithCallback0(
    const base::Callback<void(const DispatchCallback&)>& handler,
    const DispatchCallback& callback,
    const base::ListValue& list) {
  if (list.GetSize() != 0)
    return false;
  handler.Run(callback);
  return true;
}

template<class A1>
bool ParseAndHandleWithCallback1(
   const base::Callback<void(const DispatchCallback&, A1)>& handler,
   const DispatchCallback& callback,
   const base::ListValue& list) {
  if (list.GetSize() != 1)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  handler.Run(callback, a1);
  return true;
}

template<class A1, class A2>
bool ParseAndHandleWithCallback2(
    const base::Callback<void(const DispatchCallback&, A1, A2)>& handler,
    const DispatchCallback& callback,
    const base::ListValue& list) {
  if (list.GetSize() != 2)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  const base::Value* value2;
  list.Get(1, &value2);
  typename StorageTraits<A2>::StorageType a2;
  if (!GetValue(value2, &a2))
    return false;
  handler.Run(callback, a1, a2);
  return true;
}

template<class A1, class A2, class A3>
bool ParseAndHandleWithCallback3(
    const base::Callback<void(const DispatchCallback&, A1, A2, A3)>& handler,
    const DispatchCallback& callback,
    const base::ListValue& list) {
  if (list.GetSize() != 3)
    return false;
  const base::Value* value1;
  list.Get(0, &value1);
  typename StorageTraits<A1>::StorageType a1;
  if (!GetValue(value1, &a1))
    return false;
  const base::Value* value2;
  list.Get(1, &value2);
  typename StorageTraits<A2>::StorageType a2;
  if (!GetValue(value2, &a2))
    return false;
  const base::Value* value3;
  list.Get(2, &value3);
  typename StorageTraits<A3>::StorageType a3;
  if (!GetValue(value3, &a3))
    return false;
  handler.Run(callback, a1, a2, a3);
  return true;
}

} // namespace

/**
 * Dispatcher for messages sent from the frontend running in an
 * isolated renderer (chrome-devtools:// or chrome://inspect) to the embedder
 * in the browser.
 *
 * The messages are sent via InspectorFrontendHost.sendMessageToEmbedder or
 * chrome.send method accordingly.
 */
class DispatcherImpl : public DevToolsEmbedderMessageDispatcher {
 public:
  ~DispatcherImpl() override {}

  bool Dispatch(const DispatchCallback& callback,
                const std::string& method,
                const base::ListValue* params) override {
    HandlerMap::iterator it = handlers_.find(method);
    return it != handlers_.end() && it->second.Run(callback, *params);
  }

  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(),
                       Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandle0,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(A1),
                       Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandle1<A1>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1, class A2>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(A1, A2),
                       Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandle2<A1, A2>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1, class A2, class A3>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(A1, A2, A3),
                       Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandle3<A1, A2, A3>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  void RegisterHandlerWithCallback(
      const std::string& method,
      void (Delegate::*handler)(const DispatchCallback&),
      Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandleWithCallback0,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1>
  void RegisterHandlerWithCallback(
      const std::string& method,
      void (Delegate::*handler)(const DispatchCallback&, A1),
      Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandleWithCallback1<A1>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1, class A2>
  void RegisterHandlerWithCallback(
      const std::string& method,
      void (Delegate::*handler)(const DispatchCallback&, A1, A2),
      Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandleWithCallback2<A1, A2>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class A1, class A2, class A3>
  void RegisterHandlerWithCallback(
      const std::string& method,
      void (Delegate::*handler)(const DispatchCallback&, A1, A2, A3),
      Delegate* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandleWithCallback3<A1, A2, A3>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

 private:
  using Handler = base::Callback<bool(const DispatchCallback&,
                                      const base::ListValue&)>;
  using HandlerMap = std::map<std::string, Handler>;
  HandlerMap handlers_;
};

// static
DevToolsEmbedderMessageDispatcher*
DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(
    Delegate* delegate) {
  DispatcherImpl* d = new DispatcherImpl();

  d->RegisterHandler("bringToFront", &Delegate::ActivateWindow, delegate);
  d->RegisterHandler("closeWindow", &Delegate::CloseWindow, delegate);
  d->RegisterHandler("loadCompleted", &Delegate::LoadCompleted, delegate);
  d->RegisterHandler("setInspectedPageBounds",
                     &Delegate::SetInspectedPageBounds, delegate);
  d->RegisterHandler("inspectElementCompleted",
                     &Delegate::InspectElementCompleted, delegate);
  d->RegisterHandler("inspectedURLChanged",
                     &Delegate::InspectedURLChanged, delegate);
  d->RegisterHandlerWithCallback("setIsDocked",
                                 &Delegate::SetIsDocked, delegate);
  d->RegisterHandler("openInNewTab", &Delegate::OpenInNewTab, delegate);
  d->RegisterHandler("save", &Delegate::SaveToFile, delegate);
  d->RegisterHandler("append", &Delegate::AppendToFile, delegate);
  d->RegisterHandler("requestFileSystems",
                     &Delegate::RequestFileSystems, delegate);
  d->RegisterHandler("addFileSystem", &Delegate::AddFileSystem, delegate);
  d->RegisterHandler("removeFileSystem", &Delegate::RemoveFileSystem, delegate);
  d->RegisterHandler("upgradeDraggedFileSystemPermissions",
                     &Delegate::UpgradeDraggedFileSystemPermissions, delegate);
  d->RegisterHandler("indexPath", &Delegate::IndexPath, delegate);
  d->RegisterHandlerWithCallback("loadNetworkResource",
                                 &Delegate::LoadNetworkResource, delegate);
  d->RegisterHandler("stopIndexing", &Delegate::StopIndexing, delegate);
  d->RegisterHandler("searchInPath", &Delegate::SearchInPath, delegate);
  d->RegisterHandler("setWhitelistedShortcuts",
                     &Delegate::SetWhitelistedShortcuts, delegate);
  d->RegisterHandler("zoomIn", &Delegate::ZoomIn, delegate);
  d->RegisterHandler("zoomOut", &Delegate::ZoomOut, delegate);
  d->RegisterHandler("resetZoom", &Delegate::ResetZoom, delegate);
  d->RegisterHandler("setDevicesUpdatesEnabled",
                     &Delegate::SetDevicesUpdatesEnabled, delegate);
  d->RegisterHandler("sendMessageToBrowser",
                     &Delegate::SendMessageToBrowser, delegate);
  d->RegisterHandler("recordEnumeratedHistogram",
                     &Delegate::RecordEnumeratedHistogram, delegate);
  d->RegisterHandlerWithCallback("sendJsonRequest",
                                 &Delegate::SendJsonRequest, delegate);
  d->RegisterHandlerWithCallback("getPreferences",
                                 &Delegate::GetPreferences, delegate);
  d->RegisterHandler("setPreference",
                     &Delegate::SetPreference, delegate);
  d->RegisterHandler("removePreference",
                     &Delegate::RemovePreference, delegate);
  d->RegisterHandler("clearPreferences",
                     &Delegate::ClearPreferences, delegate);
  return d;
}
