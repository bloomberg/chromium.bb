// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package impl;

/*
#include "mojo/public/platform/native/system_thunks.h"
*/
import "C"
import "mojo/public/go/mojo/system"


type CoreImpl struct {
}

func (c CoreImpl) GetTimeTicksNow() int64 {
	return (int64)(C.MojoGetTimeTicksNow());
}

var lazyInstance *CoreImpl = nil;

func GetCore() system.Core {
	if lazyInstance == nil {
		lazyInstance = new(CoreImpl);
	}
	return lazyInstance;
}
