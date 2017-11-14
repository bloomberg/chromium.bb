// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ATK_HYPERLINK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ATK_HYPERLINK_H_

#include <atk/atk.h>

namespace content {

class BrowserAccessibilityAuraLinux;

G_BEGIN_DECLS

#define BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE \
  (browser_accessibility_atk_hyperlink_get_type())
#define BROWSER_ACCESSIBILITY_ATK_HYPERLINK(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE, \
                              BrowserAccessibilityAtkHyperlink))
#define BROWSER_ACCESSIBILITY_ATK_HYPERLINK_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE, \
                           BrowserAccessibilityAtkHyperlinkClass))
#define IS_BROWSER_ACCESSIBILITY_ATK_HYPERLINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE))
#define IS_BROWSER_ACCESSIBILITY_ATK_HYPERLINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE))
#define BROWSER_ACCESSIBILITY_ATK_HYPERLINK_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), BROWSER_ACCESSIBILITY_ATK_HYPERLINK_TYPE, \
                             BrowserAccessibilityAtkHyperlinkClass))

typedef struct _BrowserAccessibilityAtkHyperlink
    BrowserAccessibilityAtkHyperlink;
typedef struct _BrowserAccessibilityAtkHyperlinkClass
    BrowserAccessibilityAtkHyperlinkClass;

struct _BrowserAccessibilityAtkHyperlink {
  AtkHyperlink parent;
  BrowserAccessibilityAuraLinux* m_object;
};

struct _BrowserAccessibilityAtkHyperlinkClass {
  AtkHyperlinkClass parent_class;
};

GType browser_accessibility_atk_hyperlink_get_type(void) G_GNUC_CONST;
void browser_accessibility_atk_hyperlink_set_object(
    BrowserAccessibilityAtkHyperlink* hyperlink,
    BrowserAccessibilityAuraLinux* obj);

G_END_DECLS

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ATK_HYPERLINK_H_
