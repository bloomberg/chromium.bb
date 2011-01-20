// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/plugin_impl_mac.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/default_plugin/plugin_main.h"
#include "googleurl/src/gurl.h"
#include "grit/default_plugin_resources.h"
#include "grit/webkit_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "unicode/locid.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

// TODO(thakis): Most methods in this class are stubbed out and need to be
// implemented.

PluginInstallerImpl::PluginInstallerImpl(int16 mode)
    : image_(nil),
      command_(nil) {
}

PluginInstallerImpl::~PluginInstallerImpl() {
  [image_ release];
  [command_ release];
}

bool PluginInstallerImpl::Initialize(void* module_handle, NPP instance,
                                     NPMIMEType mime_type, int16 argc,
                                     char* argn[], char* argv[]) {
  DVLOG(1) << __FUNCTION__ << " MIME Type : " << mime_type;
  DCHECK(instance != NULL);

  if (mime_type == NULL || strlen(mime_type) == 0) {
    DLOG(WARNING) << __FUNCTION__ << " Invalid parameters passed in";
    NOTREACHED();
    return false;
  }

  instance_ = instance;
  mime_type_ = mime_type;

  command_ = [l10n_util::FixUpWindowsStyleLabel(webkit_glue::GetLocalizedString(
      IDS_DEFAULT_PLUGIN_NO_PLUGIN_AVAILABLE_MSG)) retain];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  image_ = [rb.GetNativeImageNamed(IDR_PLUGIN_ICON) retain];

  return true;
}

bool PluginInstallerImpl::NPP_SetWindow(NPWindow* window_info) {
  width_ = window_info->width;
  height_ = window_info->height;
  return true;
}

void PluginInstallerImpl::Shutdown() {
}

void PluginInstallerImpl::NewStream(NPStream* stream) {
  plugin_install_stream_ = stream;
}

void PluginInstallerImpl::DestroyStream(NPStream* stream, NPError reason) {
  if (stream == plugin_install_stream_)
    plugin_install_stream_ = NULL;
}

bool PluginInstallerImpl::WriteReady(NPStream* stream) {
  bool ready_to_accept_data = false;
  return ready_to_accept_data;
}

int32 PluginInstallerImpl::Write(NPStream* stream, int32 offset,
                                 int32 buffer_length, void* buffer) {
  return true;
}

void PluginInstallerImpl::ClearDisplay() {
}

void PluginInstallerImpl::RefreshDisplay() {
}

bool PluginInstallerImpl::CreateToolTip() {
  return true;
}

void PluginInstallerImpl::UpdateToolTip() {
}

void PluginInstallerImpl::DisplayAvailablePluginStatus() {
}

void PluginInstallerImpl::DisplayStatus(int message_resource_id) {
}

void PluginInstallerImpl::DisplayPluginDownloadFailedStatus() {
}

void PluginInstallerImpl::URLNotify(const char* url, NPReason reason) {
}

int16 PluginInstallerImpl::NPP_HandleEvent(void* event) {
  NPCocoaEvent* npp_event = static_cast<NPCocoaEvent*>(event);

  if (npp_event->type == NPCocoaEventDrawRect) {
    CGContextRef context = npp_event->data.draw.context;
    CGRect rect = CGRectMake(npp_event->data.draw.x,
                             npp_event->data.draw.y,
                             npp_event->data.draw.width,
                             npp_event->data.draw.height);
    return OnDrawRect(context, rect);
  }
  return 0;
}

std::wstring PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
    int message_id_with_placeholders,
    int messsage_id_without_placeholders,
    const std::wstring& replacement_string) {
  return L"";
}

void PluginInstallerImpl::DownloadPlugin() {
}

void PluginInstallerImpl::DownloadCancelled() {
  DisplayAvailablePluginStatus();
}

int16 PluginInstallerImpl::OnDrawRect(CGContextRef context, CGRect dirty_rect) {
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext* ns_context = [NSGraphicsContext
      graphicsContextWithGraphicsPort:context flipped:YES];
  [NSGraphicsContext setCurrentContext:ns_context];

  // Fill background.
  NSColor* bg_color = [NSColor colorWithCalibratedRed:252 / 255.0
                                                green:235 / 255.0
                                                 blue:162 / 255.0
                                                alpha:1.0];
  [bg_color setFill];
  NSRectFill(NSRectFromCGRect(dirty_rect));

  [[NSColor blackColor] set];
  NSFrameRect(NSMakeRect(0, 0, width_, height_));

  // Drag image.
  DCHECK(image_);
  if (image_) {
    NSPoint point = NSMakePoint((width_ - [image_ size].width) / 2,
                                (height_ + [image_ size].height) / 2);
    [image_ dissolveToPoint:point fraction:1.0];
  }

  // Draw text.
  NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
  [shadow setShadowColor:[NSColor colorWithDeviceWhite:1.0 alpha:0.5]];
  [shadow setShadowOffset:NSMakeSize(0, -1)];
  NSFont* font = [NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]];
  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
      font, NSFontAttributeName,
      [NSColor blackColor], NSForegroundColorAttributeName,
      shadow, NSShadowAttributeName,
      nil];

  NSSize text_size = [command_ sizeWithAttributes:attributes];
  NSPoint label_point = NSMakePoint((width_ - text_size.width) / 2,
                                    (height_ - text_size.height) / 2);
  if (image_)
    label_point.y += [image_ size].height / 2 + text_size.height / 2 + 10;
  label_point = NSMakePoint(roundf(label_point.x), roundf(label_point.y));
  [command_ drawAtPoint:label_point withAttributes:attributes];

  [NSGraphicsContext restoreGraphicsState];
  return 1;
}


void PluginInstallerImpl::ShowInstallDialog() {
}

void PluginInstallerImpl::NotifyPluginStatus(int status) {
  default_plugin::g_browser->getvalue(
      instance_,
      static_cast<NPNVariable>(
          webkit::npapi::default_plugin::kMissingPluginStatusStart + status),
      NULL);
}
