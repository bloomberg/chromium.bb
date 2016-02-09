// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/bubble/bubble_delegate.h"

class Browser;

// Subclass ChooserBubbleDelegate to implement a chooser bubble, which has
// some introductory text and a list of options that users can pick one of.
// Create an instance of your subclass and pass it to
// BubbleManager::ShowBubble() to show the bubble. Your subclass must define
// the set of options users can pick from; the actions taken after users
// select an item or press the 'Cancel' button or the bubble is closed.
// You can also override GetName() to identify the bubble you define for
// collecting metrics.
// After Select/Cancel/Close is called, this object is destroyed and call back
// into it is not allowed.
class ChooserBubbleDelegate : public BubbleDelegate {
 public:
  explicit ChooserBubbleDelegate(content::RenderFrameHost* owner);
  ~ChooserBubbleDelegate() override;

  // Since the set of options can change while the UI is visible an
  // implementation should register an observer.
  class Observer {
   public:
    // Called after the options list is initialized for the first time.
    // OnOptionsInitialized should only be called once.
    virtual void OnOptionsInitialized() = 0;

    // Called after GetOption(index) has been added to the options and the
    // newly added option is the last element in the options list. Calling
    // GetOption(index) from inside a call to OnOptionAdded will see the
    // added string since the options have already been updated.
    virtual void OnOptionAdded(size_t index) = 0;

    // Called when GetOption(index) is no longer present, and all later
    // options have been moved earlier by 1 slot. Calling GetOption(index)
    // from inside a call to OnOptionRemoved will NOT see the removed string
    // since the options have already been updated.
    virtual void OnOptionRemoved(size_t index) = 0;

   protected:
    virtual ~Observer() {}
  };

  // BubbleDelegate:
  std::string GetName() const override;
  scoped_ptr<BubbleUi> BuildBubbleUi() override;
  const content::RenderFrameHost* OwningFrame() const override;

  // The number of options users can pick from. For example, it can be
  // the number of USB/Bluetooth device names which are listed in the
  // chooser bubble so that users can grant permission.
  virtual size_t NumOptions() const = 0;

  // The |index|th option string which is listed in the chooser bubble.
  virtual const base::string16& GetOption(size_t index) const = 0;

  // These three functions are called just before this object is destroyed:

  // Called when the user selects the |index|th element from the dialog.
  virtual void Select(size_t index) = 0;

  // Called when the user presses the 'Cancel' button in the dialog.
  virtual void Cancel() = 0;

  // Called when the user clicks outside the dialog or the dialog otherwise
  // closes without the user taking an explicit action.
  virtual void Close() = 0;

  // Only one observer may be registered at a time.
  void set_observer(Observer* observer) { observer_ = observer; }
  Observer* observer() const { return observer_; }

 private:
  Browser* browser_;
  const content::RenderFrameHost* const owning_frame_;
  Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleDelegate);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_
