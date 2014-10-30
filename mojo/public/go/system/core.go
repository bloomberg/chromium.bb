// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package system

import "mojo/public/go/system/impl"

type Core interface {
	GetTimeTicksNow() int64
}

var core *impl.CoreImpl

func init() {
     core = &impl.CoreImpl{}
}

func GetCore() Core {
     return core
}