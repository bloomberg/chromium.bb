/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PPAPI_SIMPLE_PS_EVENT_H_
#define PPAPI_SIMPLE_PS_EVENT_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

/**
 * PSEvent
 *
 * The PSEvent system provides an in-order event processing mechanism.
 * As Pepper events such as input, or focus and view changes arrive on the
 * main pepper thread, they are placed on various FIFOs based on event type.
 * For any given event type, only a single thread will process those events.
 *
 * By default, the "EventThread" will receive all messages.  However any thread
 * can call PSRequestEventsType to request that all new events of that type
 * are placed on that particular thread's queue.
 *
 * This allows the developer to choose which threads handle which event types
 * while keeping all events belonging to a particular thread in order.  This
 * is useful, for example, in the case of graphics to synchronize the size
 * of the graphics context, with mouse clicks to correctly interpret location.
 */


typedef enum {
  /* Mask off all events. */
  PSE_NONE = 0,

  /* From HandleInputEvent, conatins an input resource. */
  PSE_INSTANCE_HANDLEINPUT = 1,

  /* From HandleMessage, contains a PP_Var. */
  PSE_INSTANCE_HANDLEMESSAGE = 2,

  /* From DidChangeView, contains a view resource */
  PSE_INSTANCE_DIDCHANGEVIEW = 4,

  /* From DidChangeFocus, contains a PP_Bool with the current focus state. */
  PSE_INSTANCE_DIDCHANGEFOCUS = 8,

  /* When the 3D context is lost, no resource. */
  PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST = 16,

  /* When the mouse lock is lost. */
  PSE_MOUSELOCK_MOUSELOCKLOST = 32,

  /* Enable all events. */
  PSE_ALL = -1,
} PSEventType;

typedef uint32_t PSEventTypeMask;

// Generic Event
typedef struct {
  PSEventType type;
  union {
    PP_Bool as_bool;
    PP_Resource as_resource;
    struct PP_Var as_var;
  };
} PSEvent;


/**
 * Function for queuing, acquiring, and releasing events.
 */
PSEvent* PSEventTryAcquire();
PSEvent* PSEventWaitAcquire();
void PSEventRelease(PSEvent* event);
void PSEventSetFilter(PSEventTypeMask mask);

/**
 * Creates and adds an event of the specified type to the event queue if
 * that event type is not currently filtered.
 */
void PSEventPost(PSEventType type);
void PSEventPostBool(PSEventType type, PP_Bool state);
void PSEventPostVar(PSEventType type, struct PP_Var var);
void PSEventPostResource(PSEventType type, PP_Resource resource);

EXTERN_C_END

#endif  /* PPAPI_SIMPLE_PS_EVENT_H_ */
