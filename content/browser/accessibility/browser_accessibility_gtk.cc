// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_gtk.h"

#include <gtk/gtk.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_gtk.h"
#include "content/common/accessibility_messages.h"

namespace content {

static gpointer browser_accessibility_parent_class = NULL;

static BrowserAccessibilityGtk* ToBrowserAccessibilityGtk(
    BrowserAccessibilityAtk* atk_object) {
  if (!atk_object)
    return NULL;

  return atk_object->m_object;
}

//
// AtkComponent interface.
//

static BrowserAccessibilityGtk* ToBrowserAccessibilityGtk(
    AtkComponent* atk_object) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_object))
    return NULL;

  return ToBrowserAccessibilityGtk(BROWSER_ACCESSIBILITY(atk_object));
}

static AtkObject* browser_accessibility_accessible_at_point(
    AtkComponent* component, gint x, gint y, AtkCoordType coord_type) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(component);
  if (!obj)
    return NULL;

  gfx::Point point(x, y);
  if (!obj->GetGlobalBoundsRect().Contains(point))
    return NULL;

  BrowserAccessibility* result = obj->BrowserAccessibilityForPoint(point);
  if (!result)
    return NULL;

  AtkObject* atk_result = result->ToBrowserAccessibilityGtk()->GetAtkObject();
  g_object_ref(atk_result);
  return atk_result;
}

static void browser_accessibility_get_extents(
    AtkComponent* component, gint* x, gint* y, gint* width, gint* height,
    AtkCoordType coord_type) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(component);
  if (!obj)
    return;

  gfx::Rect bounds = obj->GetGlobalBoundsRect();
  *x = bounds.x();
  *y = bounds.y();
  *width = bounds.width();
  *height = bounds.height();
}

static gboolean browser_accessibility_grab_focus(AtkComponent* component) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(component);
  if (!obj)
    return false;

  obj->manager()->SetFocus(obj, true);
  return true;
}

static void ComponentInterfaceInit(AtkComponentIface* iface) {
  iface->ref_accessible_at_point = browser_accessibility_accessible_at_point;
  iface->get_extents = browser_accessibility_get_extents;
  iface->grab_focus = browser_accessibility_grab_focus;
}

static const GInterfaceInfo ComponentInfo = {
  reinterpret_cast<GInterfaceInitFunc>(ComponentInterfaceInit), 0, 0
};

//
// AtkValue interface.
//

static BrowserAccessibilityGtk* ToBrowserAccessibilityGtk(
    AtkValue* atk_object) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_object))
    return NULL;

  return ToBrowserAccessibilityGtk(BROWSER_ACCESSIBILITY(atk_object));
}

static void browser_accessibility_get_current_value(
    AtkValue* atk_object, GValue* value) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE,
                             &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_minimum_value(
    AtkValue* atk_object, GValue* value) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE,
                             &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_maximum_value(
    AtkValue* atk_object, GValue* value) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE,
                             &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_minimum_increment(
    AtkValue* atk_object, GValue* value) {
  // TODO(dmazzoni): get the correct value from an <input type=range>.
  memset(value, 0, sizeof(*value));
  g_value_init(value, G_TYPE_FLOAT);
  g_value_set_float(value, 1.0);
}

static void ValueInterfaceInit(AtkValueIface* iface) {
  iface->get_current_value = browser_accessibility_get_current_value;
  iface->get_minimum_value = browser_accessibility_get_minimum_value;
  iface->get_maximum_value = browser_accessibility_get_maximum_value;
  iface->get_minimum_increment = browser_accessibility_get_minimum_increment;
}

static const GInterfaceInfo ValueInfo = {
  reinterpret_cast<GInterfaceInitFunc>(ValueInterfaceInit), 0, 0
};

//
// AtkObject interface
//

static BrowserAccessibilityGtk* ToBrowserAccessibilityGtk(
    AtkObject* atk_object) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_object))
    return NULL;

  return ToBrowserAccessibilityGtk(BROWSER_ACCESSIBILITY(atk_object));
}

static const gchar* browser_accessibility_get_name(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return NULL;

  return obj->GetStringAttribute(ui::AX_ATTR_NAME).c_str();
}

static const gchar* browser_accessibility_get_description(
    AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return NULL;

  return obj->GetStringAttribute(
      ui::AX_ATTR_DESCRIPTION).c_str();
}

static AtkObject* browser_accessibility_get_parent(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return NULL;
  if (obj->GetParent())
    return obj->GetParent()->ToBrowserAccessibilityGtk()->GetAtkObject();

  BrowserAccessibilityManagerGtk* manager =
      static_cast<BrowserAccessibilityManagerGtk*>(obj->manager());
  return gtk_widget_get_accessible(manager->parent_widget());
}

static gint browser_accessibility_get_n_children(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return 0;

  return obj->PlatformChildCount();
}

static AtkObject* browser_accessibility_ref_child(
    AtkObject* atk_object, gint index) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return NULL;

  if (index < 0 || index >= static_cast<gint>(obj->PlatformChildCount()))
    return NULL;

  AtkObject* result =
      obj->InternalGetChild(index)->ToBrowserAccessibilityGtk()->GetAtkObject();
  g_object_ref(result);
  return result;
}

static gint browser_accessibility_get_index_in_parent(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return 0;
  return obj->GetIndexInParent();
}

static AtkAttributeSet* browser_accessibility_get_attributes(
    AtkObject* atk_object) {
  return NULL;
}

static AtkRole browser_accessibility_get_role(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->atk_role();
}

static AtkStateSet* browser_accessibility_ref_state_set(AtkObject* atk_object) {
  BrowserAccessibilityGtk* obj = ToBrowserAccessibilityGtk(atk_object);
  if (!obj)
    return NULL;
  AtkStateSet* state_set =
      ATK_OBJECT_CLASS(browser_accessibility_parent_class)->
          ref_state_set(atk_object);
  int32 state = obj->GetState();

  if (state & (1 << ui::AX_STATE_FOCUSABLE))
    atk_state_set_add_state(state_set, ATK_STATE_FOCUSABLE);
  if (obj->manager()->GetFocus(NULL) == obj)
    atk_state_set_add_state(state_set, ATK_STATE_FOCUSED);
  if (state & (1 << ui::AX_STATE_ENABLED))
    atk_state_set_add_state(state_set, ATK_STATE_ENABLED);

  return state_set;
}

static AtkRelationSet* browser_accessibility_ref_relation_set(
    AtkObject* atk_object) {
  AtkRelationSet* relation_set =
      ATK_OBJECT_CLASS(browser_accessibility_parent_class)
          ->ref_relation_set(atk_object);
  return relation_set;
}

//
// The rest of the BrowserAccessibilityGtk code, not specific to one
// of the Atk* interfaces.
//

static void browser_accessibility_init(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(browser_accessibility_parent_class)->initialize) {
    ATK_OBJECT_CLASS(browser_accessibility_parent_class)->initialize(
        atk_object, data);
  }

  BROWSER_ACCESSIBILITY(atk_object)->m_object =
      reinterpret_cast<BrowserAccessibilityGtk*>(data);
}

static void browser_accessibility_finalize(GObject* atk_object) {
  G_OBJECT_CLASS(browser_accessibility_parent_class)->finalize(atk_object);
}

static void browser_accessibility_class_init(AtkObjectClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  browser_accessibility_parent_class = g_type_class_peek_parent(klass);

  gobject_class->finalize = browser_accessibility_finalize;
  klass->initialize = browser_accessibility_init;
  klass->get_name = browser_accessibility_get_name;
  klass->get_description = browser_accessibility_get_description;
  klass->get_parent = browser_accessibility_get_parent;
  klass->get_n_children = browser_accessibility_get_n_children;
  klass->ref_child = browser_accessibility_ref_child;
  klass->get_role = browser_accessibility_get_role;
  klass->ref_state_set = browser_accessibility_ref_state_set;
  klass->get_index_in_parent = browser_accessibility_get_index_in_parent;
  klass->get_attributes = browser_accessibility_get_attributes;
  klass->ref_relation_set = browser_accessibility_ref_relation_set;
}

GType browser_accessibility_get_type() {
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo tinfo = {
      sizeof(BrowserAccessibilityAtkClass),
      (GBaseInitFunc) 0,
      (GBaseFinalizeFunc) 0,
      (GClassInitFunc) browser_accessibility_class_init,
      (GClassFinalizeFunc) 0,
      0, /* class data */
      sizeof(BrowserAccessibilityAtk), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) 0,
      0 /* value table */
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "BrowserAccessibility", &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

static const char* GetUniqueAccessibilityTypeName(int interface_mask)
{
  // 20 characters is enough for "Chrome%x" with any integer value.
  static char name[20];
  snprintf(name, sizeof(name), "Chrome%x", interface_mask);
  return name;
}

enum AtkInterfaces {
  ATK_ACTION_INTERFACE,
  ATK_COMPONENT_INTERFACE,
  ATK_DOCUMENT_INTERFACE,
  ATK_EDITABLE_TEXT_INTERFACE,
  ATK_HYPERLINK_INTERFACE,
  ATK_HYPERTEXT_INTERFACE,
  ATK_IMAGE_INTERFACE,
  ATK_SELECTION_INTERFACE,
  ATK_TABLE_INTERFACE,
  ATK_TEXT_INTERFACE,
  ATK_VALUE_INTERFACE,
};

static int GetInterfaceMaskFromObject(BrowserAccessibilityGtk* obj) {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  int role = obj->GetRole();
  if (role == ui::AX_ROLE_PROGRESS_INDICATOR ||
      role == ui::AX_ROLE_SCROLL_BAR ||
      role == ui::AX_ROLE_SLIDER) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  return interface_mask;
}

static GType GetAccessibilityTypeFromObject(BrowserAccessibilityGtk* obj) {
  static const GTypeInfo type_info = {
    sizeof(BrowserAccessibilityAtkClass),
    (GBaseInitFunc) 0,
    (GBaseFinalizeFunc) 0,
    (GClassInitFunc) 0,
    (GClassFinalizeFunc) 0,
    0, /* class data */
    sizeof(BrowserAccessibilityAtk), /* instance size */
    0, /* nb preallocs */
    (GInstanceInitFunc) 0,
    0 /* value table */
  };

  int interface_mask = GetInterfaceMaskFromObject(obj);
  const char* atk_type_name = GetUniqueAccessibilityTypeName(interface_mask);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(BROWSER_ACCESSIBILITY_TYPE,
                                atk_type_name,
                                &type_info,
                                GTypeFlags(0));
  if (interface_mask & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &ComponentInfo);
  if (interface_mask & (1 << ATK_VALUE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &ValueInfo);

  return type;
}

BrowserAccessibilityAtk* browser_accessibility_new(
    BrowserAccessibilityGtk* obj) {
  GType type = GetAccessibilityTypeFromObject(obj);
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, 0));

  atk_object_initialize(atk_object, obj);

  return BROWSER_ACCESSIBILITY(atk_object);
}

void browser_accessibility_detach(BrowserAccessibilityAtk* atk_object) {
  atk_object->m_object = NULL;
}

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityGtk();
}

BrowserAccessibilityGtk* BrowserAccessibility::ToBrowserAccessibilityGtk() {
  return static_cast<BrowserAccessibilityGtk*>(this);
}

BrowserAccessibilityGtk::BrowserAccessibilityGtk()
    : atk_object_(NULL) {
}

BrowserAccessibilityGtk::~BrowserAccessibilityGtk() {
  browser_accessibility_detach(BROWSER_ACCESSIBILITY(atk_object_));
  if (atk_object_)
    g_object_unref(atk_object_);
}

AtkObject* BrowserAccessibilityGtk::GetAtkObject() const {
  if (!G_IS_OBJECT(atk_object_))
    return NULL;
  return atk_object_;
}

void BrowserAccessibilityGtk::PreInitialize() {
  BrowserAccessibility::PreInitialize();
  InitRoleAndState();

  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    int interface_mask = GetInterfaceMaskFromObject(this);
    if (interface_mask != interface_mask_) {
      g_object_unref(atk_object_);
      atk_object_ = NULL;
    }
  }

  if (!atk_object_) {
    interface_mask_ = GetInterfaceMaskFromObject(this);
    atk_object_ = ATK_OBJECT(browser_accessibility_new(this));
    if (this->GetParent()) {
      atk_object_set_parent(
          atk_object_,
          this->GetParent()->ToBrowserAccessibilityGtk()->GetAtkObject());
    }
  }
}

bool BrowserAccessibilityGtk::IsNative() const {
  return true;
}

void BrowserAccessibilityGtk::InitRoleAndState() {
  switch(GetRole()) {
    case ui::AX_ROLE_DOCUMENT:
    case ui::AX_ROLE_ROOT_WEB_AREA:
    case ui::AX_ROLE_WEB_AREA:
      atk_role_ = ATK_ROLE_DOCUMENT_WEB;
      break;
    case ui::AX_ROLE_GROUP:
    case ui::AX_ROLE_DIV:
      atk_role_ = ATK_ROLE_SECTION;
      break;
    case ui::AX_ROLE_BUTTON:
      atk_role_ = ATK_ROLE_PUSH_BUTTON;
      break;
    case ui::AX_ROLE_CHECK_BOX:
      atk_role_ = ATK_ROLE_CHECK_BOX;
      break;
    case ui::AX_ROLE_COMBO_BOX:
      atk_role_ = ATK_ROLE_COMBO_BOX;
      break;
    case ui::AX_ROLE_LINK:
      atk_role_ = ATK_ROLE_LINK;
      break;
    case ui::AX_ROLE_RADIO_BUTTON:
      atk_role_ = ATK_ROLE_RADIO_BUTTON;
      break;
    case ui::AX_ROLE_STATIC_TEXT:
      atk_role_ = ATK_ROLE_TEXT;
      break;
    case ui::AX_ROLE_TEXT_AREA:
      atk_role_ = ATK_ROLE_ENTRY;
      break;
    case ui::AX_ROLE_TEXT_FIELD:
      atk_role_ = ATK_ROLE_ENTRY;
      break;
    default:
      atk_role_ = ATK_ROLE_UNKNOWN;
      break;
  }
}

}  // namespace content
