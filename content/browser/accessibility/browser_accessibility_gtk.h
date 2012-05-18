// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_GTK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_GTK_H_
#pragma once

#include <atk/atk.h>

#include "base/compiler_specific.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "webkit/glue/webaccessibility.h"

using webkit_glue::WebAccessibility;

class BrowserAccessibilityGtk;
class BrowserAccessibilityManagerGtk;

G_BEGIN_DECLS

#define BROWSER_ACCESSIBILITY_TYPE (browser_accessibility_get_type())
#define BROWSER_ACCESSIBILITY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST( \
             (obj), BROWSER_ACCESSIBILITY_TYPE, BrowserAccessibilityAtk))
#define BROWSER_ACCESSIBILITY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST( \
        (klass), BROWSER_ACCESSIBILITY_TYPE, BrowserAccessibilityAtkClass))
#define IS_BROWSER_ACCESSIBILITY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_ACCESSIBILITY_TYPE))
#define IS_BROWSER_ACCESSIBILITY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BROWSER_ACCESSIBILITY_TYPE))
#define BROWSER_ACCESSIBILITY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS( \
        (obj), BROWSER_ACCESSIBILITY_TYPE, BrowserAccessibilityAtkClass))

typedef struct _BrowserAccessibilityAtk      BrowserAccessibilityAtk;
typedef struct _BrowserAccessibilityAtkClass BrowserAccessibilityAtkClass;

struct _BrowserAccessibilityAtk {
  AtkObject parent;
  BrowserAccessibilityGtk* m_object;
};

struct _BrowserAccessibilityAtkClass {
  AtkObjectClass parent_class;
};

GType browser_accessibility_get_type (void) G_GNUC_CONST;

BrowserAccessibilityAtk* browser_accessibility_new(
    BrowserAccessibilityGtk* object);

BrowserAccessibilityGtk* browser_accessibility_get_object(
    BrowserAccessibilityAtk* atk_object);

void browser_accessibility_detach (BrowserAccessibilityAtk* atk_object);

AtkObject* browser_accessibility_get_focused_element(
    BrowserAccessibilityAtk* atk_object);

G_END_DECLS

class BrowserAccessibilityGtk : public BrowserAccessibility {
 public:
  BrowserAccessibilityGtk();

  virtual ~BrowserAccessibilityGtk();

  AtkObject* GetAtkObject() const;

  AtkRole atk_role() { return atk_role_; }
  const std::string& atk_acc_name() { return atk_acc_name_; }
  const std::string& atk_acc_description() { return atk_acc_description_; }

  // BrowserAccessibility methods.
  virtual void PreInitialize() OVERRIDE;
  virtual bool IsNative() const OVERRIDE;

 private:
  virtual void InitRoleAndState();

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;

  AtkObject* atk_object_;
  AtkRole atk_role_;
  std::string atk_acc_name_;
  std::string atk_acc_description_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityGtk);
};

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_GTK_H_
