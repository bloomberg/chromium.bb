# Accessibility Overview

This document describes how accessibility is implemented throughout Chromium at
a high level.

## Concepts

The three central concepts of accessibility are:

1. The *tree*, which models the entire interface as a tree of objects, exposed
   to screenreaders or other accessibility software;
2. *Events*, which let accessibility software know that a part of the tree has
   changed somehow;
3. *Actions*, which come from accessibility software and ask the interface to
   change.

Here's an example of an accessibility tree looks like. The following HTML:

```
<select title="Select A">
  <option value="1">Option 1</option>
  <option value="2" selected>Option 2</option>
  <option value="3">Option 3</option>
</select>
```

has a generated accessibility tree like this:

```
0: AXMenuList title="Select A"
1:   AXMenuListOption title="Option 1"
2:   AXMenuListOption title="Option 2" selected
3:   AXMenuListOption title="Option 3"
```

Given that accessibility tree, an example of the events generated when selecting
"Option 1" might be:

```
AXMenuListItemUnselected 2
AXMenuListItemSelected 1
AXMenuListValueChanged 0
```

An example of a command used to change the selection from "Option 1" to "Option
3" might be:

```
AccessibilityMsg_DoDefaultAction 3
```

All three concepts are handled at several layers in Chromium.

## Blink

Blink constructs an accessibility tree (a hierarchy of [WebAXObject]s) from the
page it is rendering. WebAXObject is the public API wrapper around [AXObject],
which is the core class of Blink's accessibility tree. AXObject is an abstract
class; the most commonly used concrete subclass of it is [AXNodeObject], which
wraps a [Node]. In turn, most AXNodeObjects are actually [AXLayoutObject]s,
which wrap both a [Node] and a [LayoutObject]. Access to the LayoutObject is
important because some elements are only in the AXObject tree depending on their
visibility, geometry, linewrapping, and so on. There are some subclasses of
AXLayoutObject that implement special-case logic for specific types of Node.
There are also other subclasses of AXObject, which are mostly used for testing.

Note that not all AXLayoutObjects correspond to actual Nodes; some are synthetic
layout objects which group related inline elements or similar.

The central class responsible for dealing with accessibility events in Blink is
[AXObjectCacheImpl], which is responsible for caching the corresponding
AXObjects for Nodes or LayoutObjects. This class has many methods named
`handleFoo`, which are called throughout Blink to notify the AXObjectCacheImpl
that it may need to update its tree. Since this class is already aware of all
accessibility events in Blink, it is also responsible for relaying accessibility
events from Blink to the embedding content layer.

## The content layer

The content layer lives on both sides of the renderer/browser split. The content
layer translates WebAXObjects into [AXContentNodeData], which is a subclass of
[ui::AXNodeData]. The ui::AXNodeData class and related classes are Chromium's
cross-platform accessibility tree. The translation is implemented in
[BlinkAXTreeSource]. This translation happens on the renderer side, so the
ui::AXNodeData tree now needs to be sent to the browser, which is done by
sending [AccessibilityHostMsg_EventParams] with the payload being serialized
delta-updates to the tree, so that changes that happen on the renderer side can
be reflected on the browser side.

On the browser side, these IPCs are received by [RenderFrameHostImpl], and then
usually forwarded to [BrowserAccessibilityManager] which is responsible for:

1. Merging AXNodeData trees into one tree of [BrowserAccessibility] objects,
   by linking to other BrowserAccessibilityManagers. This is important because
   each page has its own accessibility tree, but each Chromium *window* must
   have only one accessibility tree, so trees from multiple pages need to be
   combined (possibly also with trees from Views UI).
2. Dispatching outgoing accessibility events to the platform's accessibility
   APIs. This is done in the platform-specific subclasses of
   BrowserAccessibilityManager, in a method named `NotifyAccessibilityEvent`.
3. Dispatching incoming accessibility actions to the appropriate recipient, via
   [BrowserAccessibilityDelegate]. For messages destined for a renderer,
   [RenderFrameHostImpl], which is a BrowserAccessibilityDelegate, is
   responsible for sending appropriate `AccessibilityMsg_Foo` IPCs to the
   renderer, where they will be received by [RenderAccessibilityImpl].

On Chrome OS, RenderFrameHostImpl does not route events to
BrowserAccessibilityManager at all, since there is no platform screenreader
outside Chrome to integrate with.

## Views

Views generates a [NativeViewAccessibility] for each View, which is used as the
delegate for an [AXPlatformNode] representing that View. This part is relatively
straightforward, but then the generated tree must be combined with the web
accessibility tree, which is handled by BrowserAccessibilityManager.

## WebUI

Since WebUI surfaces have renderer processes as normal, WebUI accessibility goes
through the blink-to-content-to-platform pipeline described above. Accessibility
for WebUI is largely implemented in JavaScript in [webui-js]; these classes take
care of adding ARIA attributes and so on to DOM nodes as needed.

## The Chrome OS layer

The accessibility tree is also exposed via the [chrome.automation API], which
gives extension JavaScript access to the accessibility tree, events, and
actions. This API is implemented in C++ by [AutomationInternalCustomBindings],
which is renderer-side code, and in JavaScript by the [automation API]. The API
is defined by [automation.idl], which must be kept synchronized with
[ax_enums.idl].

[AccessibilityHostMsg_EventParams]: https://cs.chromium.org/chromium/src/content/common/accessibility_messages.h?sq=package:chromium&l=75
[AutomationInternalCustomBindings]: https://cs.chromium.org/chromium/src/chrome/renderer/extensions/automation_internal_custom_bindings.h
[AXContentNodeData]: https://cs.chromium.org/chromium/src/content/common/ax_content_node_data.h
[AXLayoutObject]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/accessibility/AXLayoutObject.h
[AXNodeObject]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/accessibility/AXNodeObject.h
[AXObject]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/accessibility/AXObject.h
[AXObjectCacheImpl]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/accessibility/AXObjectCacheImpl.h
[AXPlatformNode]: https://cs.chromium.org/chromium/src/ui/accessibility/platform/ax_platform_node.h
[AXTreeSerializer]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_tree_serializer.h
[BlinkAXTreeSource]: https://cs.chromium.org/chromium/src/content/renderer/accessibility/blink_ax_tree_source.h
[BrowserAccessibility]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility.h
[BrowserAccessibilityDelegate]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility_manager.h?sq=package:chromium&l=64
[BrowserAccessibilityManager]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility_manager.h
[LayoutObject]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/layout/LayoutObject.h
[NativeViewAccessibility]: https://cs.chromium.org/chromium/src/ui/views/accessibility/native_view_accessibility.h
[Node]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/dom/Node.h
[RenderAccessibilityImpl]: https://cs.chromium.org/chromium/src/content/renderer/accessibility/render_accessibility_impl.h
[RenderFrameHostImpl]: https://cs.chromium.org/chromium/src/content/browser/frame_host/render_frame_host_impl.h
[ui::AXNodeData]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_node_data.h
[WebAXObject]: https://cs.chromium.org/chromium/src/third_party/WebKit/public/web/WebAXObject.h
[automation API]: https://cs.chromium.org/chromium/src/chrome/renderer/resources/extensions/automation
[automation.idl]: https://cs.chromium.org/chromium/src/chrome/common/extensions/api/automation.idl
[ax_enums.idl]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_enums.idl
[chrome.automation API]: https://developer.chrome.com/extensions/automation
[webui-js]: https://cs.chromium.org/chromium/src/ui/webui/resources/js/cr/ui/
