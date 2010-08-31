// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/menu_gtk.h"

#include <map>

#include "app/menus/accelerator_gtk.h"
#include "app/menus/button_menu_item_model.h"
#include "app/menus/menu_model.h"
#include "base/gtk_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/gtk_custom_menu.h"
#include "chrome/browser/gtk/gtk_custom_menu_item.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "gfx/gtk_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

using gtk_util::ConvertAcceleratorsFromWindowsStyle;
using gtk_util::RemoveWindowsStyleAccelerators;

bool MenuGtk::block_activation_ = false;

namespace {

// Sets the ID of a menu item.
void SetMenuItemID(GtkWidget* menu_item, int menu_id) {
  DCHECK_GE(menu_id, 0);

  // Add 1 to the menu_id to avoid setting zero (null) to "menu-id".
  g_object_set_data(G_OBJECT(menu_item), "menu-id",
                    GINT_TO_POINTER(menu_id + 1));
}

// Gets the ID of a menu item.
// Returns true if the menu item has an ID.
bool GetMenuItemID(GtkWidget* menu_item, int* menu_id) {
  gpointer id_ptr = g_object_get_data(G_OBJECT(menu_item), "menu-id");
  if (id_ptr != NULL) {
    *menu_id = GPOINTER_TO_INT(id_ptr) - 1;
    return true;
  }

  return false;
}

menus::MenuModel* ModelForMenuItem(GtkMenuItem* menu_item) {
  return reinterpret_cast<menus::MenuModel*>(
      g_object_get_data(G_OBJECT(menu_item), "model"));
}

void OnSubmenuShow(GtkWidget* widget, gpointer user_data) {
  gint index = GPOINTER_TO_INT(user_data);

  GtkMenuItem* item =
      GTK_MENU_ITEM(g_list_nth(GTK_MENU_SHELL(widget)->children, index)->data);
  menus::MenuModel* model = ModelForMenuItem(item);
  std::string label =
      ConvertAcceleratorsFromWindowsStyle(
          UTF16ToUTF8(model->GetLabelAt(index)));

#if GTK_CHECK_VERSION(2, 16, 0)
  gtk_menu_item_set_label(item, label.c_str());
#else
  gtk_label_set_label(GTK_LABEL(GTK_BIN(item)->child), label.c_str());
#endif
}

void OnSubmenuShowButtonMenuItem(GtkWidget* widget, GtkButton* button) {
  menus::ButtonMenuItemModel* model =
      reinterpret_cast<menus::ButtonMenuItemModel*>(
          g_object_get_data(G_OBJECT(button), "button-model"));
  int index = GPOINTER_TO_INT(g_object_get_data(
      G_OBJECT(button), "button-model-id"));

  if (model->IsLabelDynamicAt(index)) {
    std::string label =
        ConvertAcceleratorsFromWindowsStyle(
            UTF16ToUTF8(model->GetLabelAt(index)));
    gtk_button_set_label(GTK_BUTTON(button), label.c_str());
  }
  gtk_widget_set_sensitive(GTK_WIDGET(button), model->IsEnabledAt(index));
}

void SetupButtonShowHandler(GtkWidget* button,
                            GtkWidget* menu,
                            menus::ButtonMenuItemModel* model,
                            int index) {
  g_object_set_data(G_OBJECT(button), "button-model",
                    model);
  g_object_set_data(G_OBJECT(button), "button-model-id",
                    GINT_TO_POINTER(index));
  g_signal_connect(menu, "show", G_CALLBACK(OnSubmenuShowButtonMenuItem),
                   button);
}

void OnSubmenuShowButtonImage(GtkWidget* widget, GtkButton* button) {
  MenuGtk::Delegate* delegate = reinterpret_cast<MenuGtk::Delegate*>(
      g_object_get_data(G_OBJECT(button), "menu-gtk-delegate"));
  int icon_idr = GPOINTER_TO_INT(g_object_get_data(
      G_OBJECT(button), "button-image-idr"));

  GtkIconSet* icon_set = delegate->GetIconSetForId(icon_idr);
  if (icon_set) {
    gtk_button_set_image(
        button, gtk_image_new_from_icon_set(icon_set,
                                            GTK_ICON_SIZE_MENU));
  }
}

void SetupImageIcon(GtkWidget* button,
                    GtkWidget* menu,
                    int icon_idr,
                    MenuGtk::Delegate* menu_gtk_delegate) {
  g_object_set_data(G_OBJECT(button), "button-image-idr",
                    GINT_TO_POINTER(icon_idr));
  g_object_set_data(G_OBJECT(button), "menu-gtk-delegate",
                    menu_gtk_delegate);

  g_signal_connect(menu, "show", G_CALLBACK(OnSubmenuShowButtonImage), button);
}

// Popup menus may get squished if they open up too close to the bottom of the
// screen. This function takes the size of the screen, the size of the menu,
// an optional widget, the Y position of the mouse click, and adjusts the popup
// menu's Y position to make it fit if it's possible to do so.
// Returns the new Y position of the popup menu.
int CalculateMenuYPosition(const GdkRectangle* screen_rect,
                           const GtkRequisition* menu_req,
                           const GtkWidget* widget, const int y) {
  CHECK(screen_rect);
  CHECK(menu_req);
  // If the menu would run off the bottom of the screen, and there is enough
  // screen space upwards to accommodate the menu, then pop upwards. If there
  // is a widget, then also move the anchor point to the top of the widget
  // rather than the bottom.
  const int screen_top = screen_rect->y;
  const int screen_bottom = screen_rect->y + screen_rect->height;
  const int menu_bottom = y + menu_req->height;
  int alternate_y = y - menu_req->height;
  if (widget)
    alternate_y -= widget->allocation.height;
  if (menu_bottom >= screen_bottom && alternate_y >= screen_top)
    return alternate_y;
  return y;
}

}  // namespace

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 menus::MenuModel* model)
    : delegate_(delegate),
      model_(model),
      dummy_accel_group_(gtk_accel_group_new()),
      menu_(gtk_custom_menu_new()),
      factory_(this) {
  DCHECK(model);
  g_object_ref_sink(menu_);
  ConnectSignalHandlers();
  BuildMenuFromModel();
}

MenuGtk::~MenuGtk() {
  Cancel();

  gtk_widget_destroy(menu_);
  g_object_unref(menu_);

  STLDeleteContainerPointers(submenus_we_own_.begin(), submenus_we_own_.end());
  g_object_unref(dummy_accel_group_);
}

void MenuGtk::ConnectSignalHandlers() {
  // We connect afterwards because OnMenuShow calls SetMenuItemInfo, which may
  // take a long time or even start a nested message loop.
  g_signal_connect(menu_, "show", G_CALLBACK(OnMenuShow), this);
  g_signal_connect(menu_, "hide", G_CALLBACK(OnMenuHidden), this);
}

GtkWidget* MenuGtk::AppendMenuItemWithLabel(int command_id,
                                            const std::string& label) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_menu_item_new_with_mnemonic(converted_label.c_str());
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendMenuItemWithIcon(int command_id,
                                           const std::string& label,
                                           const SkBitmap& icon) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item = BuildMenuItemWithImage(converted_label, icon);
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendCheckMenuItemWithLabel(int command_id,
                                                 const std::string& label) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_check_menu_item_new_with_mnemonic(converted_label.c_str());
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendSeparator() {
  GtkWidget* menu_item = gtk_separator_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), menu_item);
  return menu_item;
}

GtkWidget* MenuGtk::AppendMenuItem(int command_id, GtkWidget* menu_item) {
  return AppendMenuItemToMenu(command_id, NULL, menu_item, menu_, true);
}

GtkWidget* MenuGtk::AppendMenuItemToMenu(int index,
                                         menus::MenuModel* model,
                                         GtkWidget* menu_item,
                                         GtkWidget* menu,
                                         bool connect_to_activate) {
  // Native menu items do their own thing, so only selectively listen for the
  // activate signal.
  if (connect_to_activate) {
    SetMenuItemID(menu_item, index);
    g_signal_connect(menu_item, "activate",
                     G_CALLBACK(OnMenuItemActivated), this);
  }

  // AppendMenuItemToMenu is used both internally when we control menu creation
  // from a model (where the model can choose to hide certain menu items), and
  // with immediate commands which don't provide the option.
  if (model) {
    if (model->IsVisibleAt(index))
      gtk_widget_show(menu_item);
  } else {
    gtk_widget_show(menu_item);
  }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  return menu_item;
}

void MenuGtk::Popup(GtkWidget* widget, GdkEvent* event) {
  DCHECK(event->type == GDK_BUTTON_PRESS)
      << "Non-button press event sent to RunMenuAt";

  Popup(widget, event->button.button, event->button.time);
}

void MenuGtk::Popup(GtkWidget* widget, gint button_type, guint32 timestamp) {
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 WidgetMenuPositionFunc,
                 widget,
                 button_type, timestamp);
}

void MenuGtk::PopupAsContext(guint32 event_time) {
  // TODO(estade): |button| value of 3 (6th argument) is not strictly true,
  // but does it matter?
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL, NULL, NULL, 3, event_time);
}

void MenuGtk::PopupAsContextAt(guint32 event_time, gfx::Point point) {
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 PointMenuPositionFunc, &point, 3, event_time);
}

void MenuGtk::PopupAsContextForStatusIcon(guint32 event_time, guint32 button,
                                          GtkStatusIcon* icon) {
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL, gtk_status_icon_position_menu,
                 icon, button, event_time);
}

void MenuGtk::PopupAsFromKeyEvent(GtkWidget* widget) {
  Popup(widget, 0, gtk_get_current_event_time());
  gtk_menu_shell_select_first(GTK_MENU_SHELL(menu_), FALSE);
}

void MenuGtk::Cancel() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

GtkWidget* MenuGtk::BuildMenuItemWithImage(const std::string& label,
                                           const SkBitmap& icon) {
  GtkWidget* menu_item =
      gtk_image_menu_item_new_with_mnemonic(label.c_str());

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);
  if (delegate_ && delegate_->AlwaysShowImages())
    gtk_util::SetAlwaysShowImage(menu_item);

  return menu_item;
}

void MenuGtk::BuildMenuFromModel() {
  BuildSubmenuFromModel(model_, menu_);
}

void MenuGtk::BuildSubmenuFromModel(menus::MenuModel* model, GtkWidget* menu) {
  std::map<int, GtkWidget*> radio_groups;
  GtkWidget* menu_item = NULL;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    SkBitmap icon;
    std::string label =
        ConvertAcceleratorsFromWindowsStyle(UTF16ToUTF8(model->GetLabelAt(i)));
    bool connect_to_activate = true;

    switch (model->GetTypeAt(i)) {
      case menus::MenuModel::TYPE_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;

      case menus::MenuModel::TYPE_CHECK:
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
        break;

      case menus::MenuModel::TYPE_RADIO: {
        std::map<int, GtkWidget*>::iterator iter =
            radio_groups.find(model->GetGroupIdAt(i));

        if (iter == radio_groups.end()) {
          menu_item = gtk_radio_menu_item_new_with_mnemonic(
              NULL, label.c_str());
          radio_groups[model->GetGroupIdAt(i)] = menu_item;
        } else {
          menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
              GTK_RADIO_MENU_ITEM(iter->second), label.c_str());
        }
        break;
      }
      case menus::MenuModel::TYPE_BUTTON_ITEM: {
        menus::ButtonMenuItemModel* button_menu_item_model =
            model->GetButtonMenuItemAt(i);
        menu_item = BuildButtomMenuItem(button_menu_item_model, menu);
        connect_to_activate = false;
        break;
      }
      case menus::MenuModel::TYPE_SUBMENU:
      case menus::MenuModel::TYPE_COMMAND:
        if (model->GetIconAt(i, &icon))
          menu_item = BuildMenuItemWithImage(label, icon);
        else
          menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());
        break;

      default:
        NOTREACHED();
    }

    if (model->GetTypeAt(i) == menus::MenuModel::TYPE_SUBMENU) {
      GtkWidget* submenu = gtk_menu_new();
      BuildSubmenuFromModel(model->GetSubmenuModelAt(i), submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    menus::AcceleratorGtk accelerator;
    if (model->GetAcceleratorAt(i, &accelerator)) {
      gtk_widget_add_accelerator(menu_item,
                                 "activate",
                                 dummy_accel_group_,
                                 accelerator.GetGdkKeyCode(),
                                 accelerator.gdk_modifier_type(),
                                 GTK_ACCEL_VISIBLE);
    }

    g_object_set_data(G_OBJECT(menu_item), "model", model);
    AppendMenuItemToMenu(i, model, menu_item, menu, connect_to_activate);

    if (model->IsLabelDynamicAt(i)) {
      g_signal_connect(menu, "show", G_CALLBACK(OnSubmenuShow),
                       GINT_TO_POINTER(i));
    }

    menu_item = NULL;
  }
}

GtkWidget* MenuGtk::BuildButtomMenuItem(menus::ButtonMenuItemModel* model,
                                        GtkWidget* menu) {
  GtkWidget* menu_item = gtk_custom_menu_item_new(
      RemoveWindowsStyleAccelerators(UTF16ToUTF8(model->label())).c_str());

  // Set up the callback to the model for when it is clicked.
  g_object_set_data(G_OBJECT(menu_item), "button-model", model);
  g_signal_connect(menu_item, "button-pushed",
                   G_CALLBACK(OnMenuButtonPressed), this);

  GtkSizeGroup* group = NULL;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    GtkWidget* button = NULL;

    switch (model->GetTypeAt(i)) {
      case menus::ButtonMenuItemModel::TYPE_SPACE: {
        gtk_custom_menu_item_add_space(GTK_CUSTOM_MENU_ITEM(menu_item));
        break;
      }
      case menus::ButtonMenuItemModel::TYPE_BUTTON: {
        button = gtk_custom_menu_item_add_button(
            GTK_CUSTOM_MENU_ITEM(menu_item),
            model->GetCommandIdAt(i));

        int icon_idr;
        if (model->GetIconAt(i, &icon_idr)) {
          SetupImageIcon(button, menu, icon_idr, delegate_);
        } else {
          gtk_button_set_label(
              GTK_BUTTON(button),
              RemoveWindowsStyleAccelerators(
                  UTF16ToUTF8(model->GetLabelAt(i))).c_str());
        }

        SetupButtonShowHandler(button, menu, model, i);
        break;
      }
      case menus::ButtonMenuItemModel::TYPE_BUTTON_LABEL: {
        button = gtk_custom_menu_item_add_button_label(
            GTK_CUSTOM_MENU_ITEM(menu_item),
            model->GetCommandIdAt(i));
        gtk_button_set_label(
            GTK_BUTTON(button),
            RemoveWindowsStyleAccelerators(
                UTF16ToUTF8(model->GetLabelAt(i))).c_str());
        SetupButtonShowHandler(button, menu, model, i);
        break;
      }
    }

    if (button && model->PartOfGroup(i)) {
      if (!group)
        group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

      gtk_size_group_add_widget(group, button);
    }
  }

  if (group) {
    g_object_unref(group);
  }

  return menu_item;
}

// static
void MenuGtk::OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu) {
  if (block_activation_)
    return;

  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (gtk_menu_item_get_submenu(menuitem))
    return;

  // The activate signal is sent to radio items as they get deselected;
  // ignore it in this case.
  if (GTK_IS_RADIO_MENU_ITEM(menuitem) &&
      !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    return;
  }

  int id;
  if (!GetMenuItemID(GTK_WIDGET(menuitem), &id))
    return;

  menus::MenuModel* model = ModelForMenuItem(menuitem);

  // The menu item can still be activated by hotkeys even if it is disabled.
  if (model->IsEnabledAt(id))
    menu->ExecuteCommand(model, id);
}

void MenuGtk::OnMenuButtonPressed(GtkMenuItem* menu_item, int command_id,
                                  MenuGtk* menu) {
  menus::ButtonMenuItemModel* model =
      reinterpret_cast<menus::ButtonMenuItemModel*>(
          g_object_get_data(G_OBJECT(menu_item), "button-model"));
  if (model && model->IsCommandIdEnabled(command_id)) {
    if (menu->delegate_)
      menu->delegate_->CommandWillBeExecuted();

    model->ActivatedCommand(command_id);
  }
}

// static
void MenuGtk::WidgetMenuPositionFunc(GtkMenu* menu,
                                     int* x,
                                     int* y,
                                     gboolean* push_in,
                                     void* void_widget) {
  GtkWidget* widget = GTK_WIDGET(void_widget);
  GtkRequisition menu_req;

  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  gdk_window_get_origin(widget->window, x, y);
  GdkScreen *screen = gtk_widget_get_screen(widget);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor,
                                  &screen_rect);

  if (GTK_WIDGET_NO_WINDOW(widget)) {
    *x += widget->allocation.x;
    *y += widget->allocation.y;
  }
  *y += widget->allocation.height;

  bool start_align =
    !!g_object_get_data(G_OBJECT(widget), "left-align-popup");
  if (base::i18n::IsRTL())
    start_align = !start_align;

  if (!start_align)
    *x += widget->allocation.width - menu_req.width;

  *y = CalculateMenuYPosition(&screen_rect, &menu_req, widget, *y);

  *push_in = FALSE;
}

// static
void MenuGtk::PointMenuPositionFunc(GtkMenu* menu,
                                    int* x,
                                    int* y,
                                    gboolean* push_in,
                                    gpointer userdata) {
  *push_in = TRUE;

  gfx::Point* point = reinterpret_cast<gfx::Point*>(userdata);
  *x = point->x();
  *y = point->y();

  GtkRequisition menu_req;
  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);
  GdkScreen* screen;
  gdk_display_get_pointer(gdk_display_get_default(), &screen, NULL, NULL, NULL);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor, &screen_rect);

  *y = CalculateMenuYPosition(&screen_rect, &menu_req, NULL, *y);
}

void MenuGtk::UpdateMenu() {
  gtk_container_foreach(GTK_CONTAINER(menu_), SetMenuItemInfo, this);
}

void MenuGtk::ExecuteCommand(menus::MenuModel* model, int id) {
  if (delegate_)
    delegate_->CommandWillBeExecuted();

  model->ActivatedAt(id);
}

// static
void MenuGtk::OnMenuShow(GtkWidget* widget, MenuGtk* menu) {
  MessageLoop::current()->PostTask(FROM_HERE,
      menu->factory_.NewRunnableMethod(&MenuGtk::UpdateMenu));
}

// static
void MenuGtk::OnMenuHidden(GtkWidget* widget, MenuGtk* menu) {
  if (menu->delegate_)
    menu->delegate_->StoppedShowing();
}

// static
void MenuGtk::SetMenuItemInfo(GtkWidget* widget, gpointer userdata) {
  if (GTK_IS_SEPARATOR_MENU_ITEM(widget)) {
    // We need to explicitly handle this case because otherwise we'll ask the
    // menu delegate about something with an invalid id.
    return;
  }

  int id;
  if (!GetMenuItemID(widget, &id))
    return;

  menus::MenuModel* model = ModelForMenuItem(GTK_MENU_ITEM(widget));
  if (!model) {
    // If we're not providing the sub menu, then there's no model.  For
    // example, the IME submenu doesn't have a model.
    return;
  }

  if (GTK_IS_CHECK_MENU_ITEM(widget)) {
    GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);

    // gtk_check_menu_item_set_active() will send the activate signal. Touching
    // the underlying "active" property will also call the "activate" handler
    // for this menu item. So we prevent the "activate" handler from
    // being called while we set the checkbox.
    // Why not use one of the glib signal-blocking functions?  Because when we
    // toggle a radio button, it will deactivate one of the other radio buttons,
    // which we don't have a pointer to.
    // Wny not make this a member variable?  Because "menu" is a pointer to the
    // root of the MenuGtk and we want to disable *all* MenuGtks, including
    // submenus.
    block_activation_ = true;
    gtk_check_menu_item_set_active(item, model->IsItemCheckedAt(id));
    block_activation_ = false;
  }

  if (GTK_IS_MENU_ITEM(widget)) {
    gtk_widget_set_sensitive(widget, model->IsEnabledAt(id));

    if (model->IsVisibleAt(id))
      gtk_widget_show(widget);
    else
      gtk_widget_hide(widget);

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
    if (submenu) {
      gtk_container_foreach(GTK_CONTAINER(submenu), &SetMenuItemInfo,
                            userdata);
    }
  }
}
