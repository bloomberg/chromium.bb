// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package tests

import "mojo/go/system/embedder"
import "mojo/go/system/impl"
import "mojo/public/go/mojo/system"
import "testing"

func Init() {
	embedder.InitializeMojoEmbedder();
}

func TestGetTimeTicksNow(t *testing.T) {
	Init();
	var c system.Core = impl.GetCore();
	x := c.GetTimeTicksNow();
	if (x < 10) {
		t.Error("Invalid GetTimeTicksNow return value");
	}
}
