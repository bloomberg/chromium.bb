// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WUG_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_WEBUI_WUG_VIEW_MODEL_H_

#include <vector>

#include "base/macros.h"
#include "components/login/screens/screen_context.h"
#include "components/webui_generator/export.h"

namespace base {
class DictionaryValue;
}

namespace webui_generator {

using Context = login::ScreenContext;
class View;

// Base class for view-model. Usually View is responsible for creating and
// initializing instances of this class.
class WUG_EXPORT ViewModel {
 public:
  ViewModel();
  virtual ~ViewModel();

  void SetView(View* view);

  // Called by view to notify that children are ready.
  void OnChildrenReady();

  // Called by view when it became bound (meaning that all changes in context
  // will be reflected in view right after they are made).
  virtual void OnViewBound() = 0;

  // Notifies view-model about context changes happended on view side.
  // View-model corrects its copy of context according to changes in |diff|.
  void UpdateContext(const base::DictionaryValue& diff);

  // Notifies view-model about |event| sent from view.
  virtual void OnEvent(const std::string& event);

 protected:
  // Scoped context editor, which automatically commits all pending
  // context changes on destruction.
  class ContextEditor {
   public:
    using KeyType = ::login::ScreenContext::KeyType;
    using String16List = ::login::String16List;
    using StringList = ::login::StringList;

    explicit ContextEditor(ViewModel& screen);
    ~ContextEditor();

    const ContextEditor& SetBoolean(const KeyType& key, bool value) const;
    const ContextEditor& SetInteger(const KeyType& key, int value) const;
    const ContextEditor& SetDouble(const KeyType& key, double value) const;
    const ContextEditor& SetString(const KeyType& key,
                                   const std::string& value) const;
    const ContextEditor& SetString(const KeyType& key,
                                   const base::string16& value) const;
    const ContextEditor& SetStringList(const KeyType& key,
                                       const StringList& value) const;
    const ContextEditor& SetString16List(const KeyType& key,
                                         const String16List& value) const;

   private:
    ViewModel& view_model_;
    Context& context_;
  };

  // This method is called in a time appropriate for initialization. At this
  // point it is allowed to make changes in the context, but it is not allowed
  // to access child view-models.
  virtual void Initialize() = 0;

  // This method is called when all children are ready.
  virtual void OnAfterChildrenReady() = 0;

  // This method is called after context was changed on view side.
  // |chanaged_keys| contains a list of keys of changed fields.
  virtual void OnContextChanged(const std::vector<std::string>& changed_keys) {}

  Context& context() { return context_; }
  const Context& context() const { return context_; }

  View* view() const { return view_; }

  // Returns scoped context editor. The editor or it's copies should not outlive
  // current BaseScreen instance.
  ContextEditor GetContextEditor();

  // Notifies view about changes made in context.
  void CommitContextChanges();

 private:
  Context context_;
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewModel);
};

}  // namespace webui_generator

#endif  // CHROME_BROWSER_UI_WEBUI_WUG_VIEW_MODEL_H_
