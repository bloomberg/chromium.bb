// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_AX_OBJECT_PROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_AX_OBJECT_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "v8/include/v8-util.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
}

namespace content {

class WebAXObjectProxy : public gin::Wrappable<WebAXObjectProxy> {
 public:
  class Factory {
   public:
    virtual ~Factory() { }
    virtual v8::Handle<v8::Object> GetOrCreate(
        const blink::WebAXObject& object) = 0;
  };

  static gin::WrapperInfo kWrapperInfo;

  WebAXObjectProxy(const blink::WebAXObject& object, Factory* factory);
  virtual ~WebAXObjectProxy();

  // gin::Wrappable:
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  virtual v8::Handle<v8::Object> GetChildAtIndex(unsigned index);
  virtual bool IsRoot() const;
  bool IsEqualToObject(const blink::WebAXObject& object);

  void NotificationReceived(blink::WebFrame* frame,
                            const std::string& notification_name);
  void Reset();

 protected:
  const blink::WebAXObject& accessibility_object() const {
    return accessibility_object_;
  }

  Factory* factory() const { return factory_; }

 private:
  friend class WebAXObjectProxyBindings;

  // Bound properties.
  std::string Role();
  std::string Title();
  std::string Description();
  std::string HelpText();
  std::string StringValue();
  int X();
  int Y();
  int Width();
  int Height();
  int IntValue();
  int MinValue();
  int MaxValue();
  std::string ValueDescription();
  int ChildrenCount();
  int InsertionPointLineNumber();
  std::string SelectedTextRange();
  bool IsEnabled();
  bool IsRequired();
  bool IsFocused();
  bool IsFocusable();
  bool IsSelected();
  bool IsSelectable();
  bool IsMultiSelectable();
  bool IsSelectedOptionActive();
  bool IsExpanded();
  bool IsChecked();
  bool IsVisible();
  bool IsOffScreen();
  bool IsCollapsed();
  bool HasPopup();
  bool IsValid();
  bool IsReadOnly();
  std::string Orientation();
  int ClickPointX();
  int ClickPointY();
  int32_t RowCount();
  int32_t ColumnCount();
  bool IsClickable();

  // Bound methods.
  std::string AllAttributes();
  std::string AttributesOfChildren();
  int LineForIndex(int index);
  std::string BoundsForRange(int start, int end);
  v8::Handle<v8::Object> ChildAtIndex(int index);
  v8::Handle<v8::Object> ElementAtPoint(int x, int y);
  v8::Handle<v8::Object> TableHeader();
  std::string RowIndexRange();
  std::string ColumnIndexRange();
  v8::Handle<v8::Object> CellForColumnAndRow(int column, int row);
  v8::Handle<v8::Object> TitleUIElement();
  void SetSelectedTextRange(int selection_start, int length);
  bool IsAttributeSettable(const std::string& attribute);
  bool IsPressActionSupported();
  bool IsIncrementActionSupported();
  bool IsDecrementActionSupported();
  v8::Handle<v8::Object> ParentElement();
  void Increment();
  void Decrement();
  void ShowMenu();
  void Press();
  bool IsEqual(v8::Handle<v8::Object> proxy);
  void SetNotificationListener(v8::Handle<v8::Function> callback);
  void UnsetNotificationListener();
  void TakeFocus();
  void ScrollToMakeVisible();
  void ScrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);
  void ScrollToGlobalPoint(int x, int y);
  int WordStart(int character_index);
  int WordEnd(int character_index);

  blink::WebAXObject accessibility_object_;
  Factory* factory_;

  v8::Persistent<v8::Function> notification_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebAXObjectProxy);
};

class RootWebAXObjectProxy : public WebAXObjectProxy {
 public:
  RootWebAXObjectProxy(const blink::WebAXObject&, Factory*);

  virtual v8::Handle<v8::Object> GetChildAtIndex(unsigned index) OVERRIDE;
  virtual bool IsRoot() const OVERRIDE;
};


// Provides simple lifetime management of the WebAXObjectProxy instances: all
// WebAXObjectProxys ever created from the controller are stored in a list and
// cleared explicitly.
class WebAXObjectProxyList : public WebAXObjectProxy::Factory {
 public:
  WebAXObjectProxyList();
  virtual ~WebAXObjectProxyList();

  void Clear();
  virtual v8::Handle<v8::Object> GetOrCreate(
      const blink::WebAXObject&) OVERRIDE;
  v8::Handle<v8::Object> CreateRoot(const blink::WebAXObject&);

 private:
  typedef v8::PersistentValueVector<v8::Object> ElementList;
  ElementList elements_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_AX_OBJECT_PROXY_H_
