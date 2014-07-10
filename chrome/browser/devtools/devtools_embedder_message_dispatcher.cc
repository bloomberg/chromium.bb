// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include "base/bind.h"
#include "base/values.h"

namespace {

bool GetValue(const base::ListValue& list, int pos, std::string& value) {
  return list.GetString(pos, &value);
}

bool GetValue(const base::ListValue& list, int pos, int& value) {
  return list.GetInteger(pos, &value);
}

bool GetValue(const base::ListValue& list, int pos, bool& value) {
  return list.GetBoolean(pos, &value);
}

bool GetValue(const base::ListValue& list, int pos, gfx::Insets& insets) {
  const base::DictionaryValue* dict;
  if (!list.GetDictionary(pos, &dict))
    return false;
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;
  if (!dict->GetInteger("top", &top) ||
      !dict->GetInteger("left", &left) ||
      !dict->GetInteger("bottom", &bottom) ||
      !dict->GetInteger("right", &right))
    return false;
  insets.Set(top, left, bottom, right);
  return true;
}

bool GetValue(const base::ListValue& list, int pos, gfx::Size& size) {
  const base::DictionaryValue* dict;
  if (!list.GetDictionary(pos, &dict))
    return false;
  int width = 0;
  int height = 0;
  if (!dict->GetInteger("width", &width) ||
      !dict->GetInteger("height", &height))
    return false;
  size.SetSize(width, height);
  return true;
}

bool GetValue(const base::ListValue& list, int pos, gfx::Rect& rect) {
  const base::DictionaryValue* dict;
  if (!list.GetDictionary(pos, &dict))
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
  rect.SetRect(x, y, width, height);
  return true;
}

template <typename T>
struct StorageTraits {
  typedef T StorageType;
};

template <typename T>
struct StorageTraits<const T&> {
  typedef T StorageType;
};

template <class A>
class Argument {
 public:
  typedef typename StorageTraits<A>::StorageType ValueType;

  Argument(const base::ListValue& list, int pos) {
    valid_ = GetValue(list, pos, value_);
  }

  ValueType value() const { return value_; }
  bool valid() const { return valid_; }

 private:
  ValueType value_;
  bool valid_;
};

bool ParseAndHandle0(const base::Callback<void(void)>& handler,
                     const base::ListValue& list) {
  if (list.GetSize() != 0)
      return false;
  handler.Run();
  return true;
}

template <class A1>
bool ParseAndHandle1(const base::Callback<void(A1)>& handler,
                     const base::ListValue& list) {
  if (list.GetSize() != 1)
    return false;
  Argument<A1> arg1(list, 0);
  if (!arg1.valid())
    return false;
  handler.Run(arg1.value());
  return true;
}

template <class A1, class A2>
bool ParseAndHandle2(const base::Callback<void(A1, A2)>& handler,
                     const base::ListValue& list) {
  if (list.GetSize() != 2)
    return false;
  Argument<A1> arg1(list, 0);
  if (!arg1.valid())
    return false;
  Argument<A2> arg2(list, 1);
  if (!arg2.valid())
    return false;
  handler.Run(arg1.value(), arg2.value());
  return true;
}

template <class A1, class A2, class A3>
bool ParseAndHandle3(const base::Callback<void(A1, A2, A3)>& handler,
                     const base::ListValue& list) {
  if (list.GetSize() != 3)
    return false;
  Argument<A1> arg1(list, 0);
  if (!arg1.valid())
    return false;
  Argument<A2> arg2(list, 1);
  if (!arg2.valid())
    return false;
  Argument<A3> arg3(list, 2);
  if (!arg3.valid())
    return false;
  handler.Run(arg1.value(), arg2.value(), arg3.value());
  return true;
}

template <class A1, class A2, class A3, class A4>
bool ParseAndHandle4(const base::Callback<void(A1, A2, A3, A4)>& handler,
                     const base::ListValue& list) {
  if (list.GetSize() != 4)
    return false;
  Argument<A1> arg1(list, 0);
  if (!arg1.valid())
    return false;
  Argument<A2> arg2(list, 1);
  if (!arg2.valid())
    return false;
  Argument<A3> arg3(list, 2);
  if (!arg3.valid())
    return false;
  Argument<A4> arg4(list, 3);
  if (!arg4.valid())
    return false;
  handler.Run(arg1.value(), arg2.value(), arg3.value(), arg4.value());
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
  virtual ~DispatcherImpl() {}

  virtual bool Dispatch(const std::string& method,
                        const base::ListValue* params,
                        std::string* error) OVERRIDE {
    HandlerMap::iterator it = handlers_.find(method);
    if (it == handlers_.end())
      return false;

    if (it->second.Run(*params))
      return true;

    if (error)
      *error = "Invalid frontend host message parameters: " + method;
    return false;
  }

  typedef base::Callback<bool(const base::ListValue&)> Handler;
  void RegisterHandler(const std::string& method, const Handler& handler) {
    handlers_[method] = handler;
  }

  template<class T>
  void RegisterHandler(const std::string& method,
                       void (T::*handler)(), T* delegate) {
    handlers_[method] = base::Bind(&ParseAndHandle0,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class T, class A1>
  void RegisterHandler(const std::string& method,
                       void (T::*handler)(A1), T* delegate) {
    handlers_[method] = base::Bind(ParseAndHandle1<A1>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class T, class A1, class A2>
  void RegisterHandler(const std::string& method,
                       void (T::*handler)(A1, A2), T* delegate) {
    handlers_[method] = base::Bind(ParseAndHandle2<A1, A2>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class T, class A1, class A2, class A3>
  void RegisterHandler(const std::string& method,
                       void (T::*handler)(A1, A2, A3), T* delegate) {
    handlers_[method] = base::Bind(ParseAndHandle3<A1, A2, A3>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

  template<class T, class A1, class A2, class A3, class A4>
  void RegisterHandler(const std::string& method,
                       void (T::*handler)(A1, A2, A3, A4), T* delegate) {
    handlers_[method] = base::Bind(ParseAndHandle4<A1, A2, A3, A4>,
                                   base::Bind(handler,
                                              base::Unretained(delegate)));
  }

 private:
  typedef std::map<std::string, Handler> HandlerMap;
  HandlerMap handlers_;
};


DevToolsEmbedderMessageDispatcher*
    DevToolsEmbedderMessageDispatcher::createForDevToolsFrontend(
        Delegate* delegate) {
  DispatcherImpl* d = new DispatcherImpl();

  d->RegisterHandler("bringToFront", &Delegate::ActivateWindow, delegate);
  d->RegisterHandler("closeWindow", &Delegate::CloseWindow, delegate);
  d->RegisterHandler("setInspectedPageBounds",
                     &Delegate::SetInspectedPageBounds, delegate);
  d->RegisterHandler("setContentsResizingStrategy",
                     &Delegate::SetContentsResizingStrategy, delegate);
  d->RegisterHandler("inspectElementCompleted",
                     &Delegate::InspectElementCompleted, delegate);
  d->RegisterHandler("inspectedURLChanged",
                     &Delegate::InspectedURLChanged, delegate);
  d->RegisterHandler("moveWindowBy", &Delegate::MoveWindow, delegate);
  d->RegisterHandler("setIsDocked", &Delegate::SetIsDocked, delegate);
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
  d->RegisterHandler("stopIndexing", &Delegate::StopIndexing, delegate);
  d->RegisterHandler("searchInPath", &Delegate::SearchInPath, delegate);
  d->RegisterHandler("setWhitelistedShortcuts",
                     &Delegate::SetWhitelistedShortcuts, delegate);
  d->RegisterHandler("zoomIn", &Delegate::ZoomIn, delegate);
  d->RegisterHandler("zoomOut", &Delegate::ZoomOut, delegate);
  d->RegisterHandler("resetZoom", &Delegate::ResetZoom, delegate);
  d->RegisterHandler("openUrlOnRemoteDeviceAndInspect",
                     &Delegate::OpenUrlOnRemoteDeviceAndInspect, delegate);
  d->RegisterHandler("setDeviceCountUpdatesEnabled",
                     &Delegate::SetDeviceCountUpdatesEnabled, delegate);
  d->RegisterHandler("setDevicesUpdatesEnabled",
                     &Delegate::SetDevicesUpdatesEnabled, delegate);
  return d;
}
