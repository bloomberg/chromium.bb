// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

#include "base/compiler_specific.h"
#import "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/tracked_objects.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

namespace {

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* GetMenuItemByID(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (model->GetCommandIdAt(i) == command_id)
      return item;

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu && [item hasSubmenu]) {
      NSMenuItem* subitem = GetMenuItemByID(submenu,
                                            [item submenu],
                                            command_id);
      if (subitem)
        return subitem;
    }
  }
  return nil;
}

}  // namespace

// OSX implemenation of the ToolkitDelegate.
// This simply (re)delegates calls to RVContextMenuMac because they do not
// have to be componentized.
class ToolkitDelegateMac : public RenderViewContextMenuBase::ToolkitDelegate {
 public:
  explicit ToolkitDelegateMac(RenderViewContextMenuMac* context_menu)
      : context_menu_(context_menu) {}
  ~ToolkitDelegateMac() override {}

 private:
  // ToolkitDelegate:
  void Init(ui::SimpleMenuModel* menu_model) override {
    context_menu_->InitToolkitMenu();
  }
  void Cancel() override { context_menu_->CancelToolkitMenu(); }
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const base::string16& title) override {
    context_menu_->UpdateToolkitMenuItem(
        command_id, enabled, hidden, title);
  }

  RenderViewContextMenuMac* context_menu_;
  DISALLOW_COPY_AND_ASSIGN(ToolkitDelegateMac);
};

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.

RenderViewContextMenuMac::RenderViewContextMenuMac(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params,
    NSView* parent_view)
    : RenderViewContextMenu(render_frame_host, params),
      speech_submenu_model_(this),
      bidi_submenu_model_(this),
      parent_view_(parent_view) {
  scoped_ptr<ToolkitDelegate> delegate(new ToolkitDelegateMac(this));
  set_toolkit_delegate(delegate.Pass());
}

RenderViewContextMenuMac::~RenderViewContextMenuMac() {
}

void RenderViewContextMenuMac::Show() {
  menu_controller_.reset(
      [[MenuController alloc] initWithModel:&menu_model_
                     useWithPopUpButtonCell:NO]);

  gfx::Point params_position(params_.x, params_.y);
  params_position += RenderViewContextMenu::GetOffset(GetRenderFrameHost());

  // Synthesize an event for the click, as there is no certainty that
  // [NSApp currentEvent] will return a valid event.
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view_ window];
  NSPoint position =
      NSMakePoint(params_position.x(),
                  NSHeight([parent_view_ bounds]) - params_position.y());
  position = [parent_view_ convertPoint:position toView:nil];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                           location:position
                                      modifierFlags:NSRightMouseDownMask
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  {
    // Make sure events can be pumped while the menu is up.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sendingEventScoper;

    // Use task stopwatch to exclude the loop run time from the current task, if
    // any.
    tracked_objects::TaskStopwatch stopwatch;
    stopwatch.Start();
    // Show the menu.
    [NSMenu popUpContextMenu:[menu_controller_ menu]
                   withEvent:clickEvent
                     forView:parent_view_];
    stopwatch.Stop();
  }
}

void RenderViewContextMenuMac::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_LOOK_UP:
      LookUpInDictionary();
      break;

    case IDC_CONTENT_CONTEXT_SPEECH_START_SPEAKING:
      StartSpeaking();
      break;

    case IDC_CONTENT_CONTEXT_SPEECH_STOP_SPEAKING:
      StopSpeaking();
      break;

    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      break;

    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      content::RenderViewHost* view_host = GetRenderViewHost();
      blink::WebTextDirection dir = blink::WebTextDirectionLeftToRight;
      if (command_id == IDC_WRITING_DIRECTION_RTL)
        dir = blink::WebTextDirectionRightToLeft;
      view_host->GetWidget()->UpdateTextDirection(dir);
      view_host->GetWidget()->NotifyTextDirection();
      RenderViewContextMenu::RecordUsedItem(command_id);
      break;
    }

    default:
      RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
      break;
  }
}

bool RenderViewContextMenuMac::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      return params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemChecked;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemChecked;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemChecked;

    default:
      return RenderViewContextMenu::IsCommandIdChecked(command_id);
  }
}

bool RenderViewContextMenuMac::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_LOOK_UP:
      // This is OK because the menu is not shown when it isn't
      // appropriate.
      return true;

    case IDC_CONTENT_CONTEXT_SPEECH_START_SPEAKING:
      // This is OK because the menu is not shown when it isn't
      // appropriate.
      return true;

    case IDC_CONTENT_CONTEXT_SPEECH_STOP_SPEAKING: {
      content::RenderWidgetHostView* view =
          GetRenderViewHost()->GetWidget()->GetView();
      return view && view->IsSpeaking();
    }

    case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
      return params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemEnabled;

    default:
      return RenderViewContextMenu::IsCommandIdEnabled(command_id);
  }
}

bool RenderViewContextMenuMac::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void RenderViewContextMenuMac::AppendPlatformEditableItems() {
  // OS X provides a contextual menu to set writing direction for BiDi
  // languages.
  // This functionality is exposed as a keyboard shortcut on Windows & Linux.
  AppendBidiSubMenu();
}

void RenderViewContextMenuMac::InitToolkitMenu() {
  if (params_.selection_text.empty())
    return;

  if (params_.link_url.is_empty()) {
    // In case the user has selected a word that triggers spelling suggestions,
    // show the dictionary lookup under the group that contains the command to
    // “Add to Dictionary.”
    int index = menu_model_.GetIndexOfCommandId(
        IDC_SPELLCHECK_ADD_TO_DICTIONARY);
    if (index < 0) {
      index = 0;
    } else {
      while (menu_model_.GetTypeAt(index) != ui::MenuModel::TYPE_SEPARATOR) {
        index++;
      }
      index += 1; // Place it below the separator.
    }

    base::string16 printable_selection_text = PrintableSelectionText();
    EscapeAmpersands(&printable_selection_text);
    menu_model_.InsertItemAt(
        index++,
        IDC_CONTENT_CONTEXT_LOOK_UP,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP,
                                   printable_selection_text));
    menu_model_.InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
  }

  content::RenderWidgetHostView* view =
      GetRenderViewHost()->GetWidget()->GetView();
  if (view && view->SupportsSpeech()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    speech_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_SPEECH_START_SPEAKING,
        IDS_SPEECH_START_SPEAKING_MAC);
    speech_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_SPEECH_STOP_SPEAKING,
        IDS_SPEECH_STOP_SPEAKING_MAC);
    menu_model_.AddSubMenu(
        IDC_CONTENT_CONTEXT_SPEECH_MENU,
        l10n_util::GetStringUTF16(IDS_SPEECH_MAC),
        &speech_submenu_model_);
  }
}

void RenderViewContextMenuMac::CancelToolkitMenu() {
  [menu_controller_ cancel];
}

void RenderViewContextMenuMac::UpdateToolkitMenuItem(
    int command_id,
    bool enabled,
    bool hidden,
    const base::string16& title) {
  NSMenuItem* item = GetMenuItemByID(&menu_model_, [menu_controller_ menu],
                                     command_id);
  if (!item)
    return;

  // Update the returned NSMenuItem directly so we can update it immediately.
  [item setEnabled:enabled];
  [item setTitle:base::SysUTF16ToNSString(title)];
  [item setHidden:hidden];
  [[item menu] itemChanged:item];
}

void RenderViewContextMenuMac::AppendBidiSubMenu() {
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_DEFAULT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_LTR,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_RTL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL));

  menu_model_.AddSubMenu(
      IDC_WRITING_DIRECTION_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU),
      &bidi_submenu_model_);
}

void RenderViewContextMenuMac::LookUpInDictionary() {
  content::RenderWidgetHostView* view =
      GetRenderViewHost()->GetWidget()->GetView();
  if (view)
    view->ShowDefinitionForSelection();
}

void RenderViewContextMenuMac::StartSpeaking() {
  content::RenderWidgetHostView* view =
      GetRenderViewHost()->GetWidget()->GetView();
  if (view)
    view->SpeakSelection();
}

void RenderViewContextMenuMac::StopSpeaking() {
  content::RenderWidgetHostView* view =
      GetRenderViewHost()->GetWidget()->GetView();
  if (view)
    view->StopSpeaking();
}
