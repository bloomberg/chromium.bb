// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"

#include "ios/web/public/web_thread.h"

@implementation MDCPalette (CrAdditions)

namespace {
static MDCPalette* g_bluePalette = nil;
static MDCPalette* g_redPalette = nil;
static MDCPalette* g_greenPalette = nil;
static MDCPalette* g_yellowPalette = nil;
}

+ (MDCPalette*)cr_bluePalette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  if (!g_bluePalette)
    return [MDCPalette bluePalette];
  return g_bluePalette;
}

+ (MDCPalette*)cr_redPalette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  if (!g_redPalette)
    return [MDCPalette redPalette];
  return g_redPalette;
}

+ (MDCPalette*)cr_greenPalette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  if (!g_greenPalette)
    return [MDCPalette greenPalette];
  return g_greenPalette;
}

+ (MDCPalette*)cr_yellowPalette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  if (!g_yellowPalette)
    return [MDCPalette yellowPalette];
  return g_yellowPalette;
}

+ (void)cr_setBluePalette:(MDCPalette*)palette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  [g_bluePalette autorelease];
  g_bluePalette = [palette retain];
}

+ (void)cr_setRedPalette:(MDCPalette*)palette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  [g_redPalette autorelease];
  g_redPalette = [palette retain];
}

+ (void)cr_setGreenPalette:(MDCPalette*)palette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  [g_greenPalette autorelease];
  g_greenPalette = [palette retain];
}

+ (void)cr_setYellowPalette:(MDCPalette*)palette {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  [g_yellowPalette autorelease];
  g_yellowPalette = [palette retain];
}

@end
