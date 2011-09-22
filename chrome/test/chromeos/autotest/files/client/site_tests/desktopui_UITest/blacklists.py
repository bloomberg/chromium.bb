# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(rkc): Tests currently failing on the PFQ - resolve these.
# http://code.google.com/p/chromium-os/issues/detail?id=20128
blacklist = [
    # control$
    'ResourceDispatcherTest.SniffHTMLWithNoContentType',
    'ResourceDispatcherTest.SniffNoContentTypeNoData',
    'ResourceDispatcherTest.ContentDispositionEmpty',
    'ResourceDispatcherTest.ContentDispositionInline',
    'ResourceDispatcherTest.CrossSiteNavigationNonBuffered',

    # control.one$
    'PPAPITest.Broker',
    'PPAPITest.Core',
    'PPAPITest.CursorControl',
    'PPAPITest.Instance',
    'PPAPITest.Graphics2D',
    'PPAPITest.ImageData',
    'PPAPITest.Buffer',
    'PPAPITest.URLLoader',
    'PPAPITest.PaintAggregator',
    'PPAPITest.Scrollbar',
    'PPAPITest.URLUtil',
    'PPAPITest.CharSet',
    'PPAPITest.Var',
    'PPAPITest.VarDeprecated',
    'PPAPITest.PostMessage',
    'PPAPITest.Memory',
    'PPAPITest.QueryPolicy',
    'PPAPITest.VideoDecoder',
    'PPAPITest.FileRef',
    'PPAPITest.FileSystem',
    'PPAPITest.UMA',
    'OutOfProcessPPAPITest.Broker',
    'OutOfProcessPPAPITest.Core',
    'OutOfProcessPPAPITest.CursorControl',
    'OutOfProcessPPAPITest.ImageData',
    'OutOfProcessPPAPITest.Buffer',
    'OutOfProcessPPAPITest.PaintAggregator',
    'OutOfProcessPPAPITest.URLUtil',
    'OutOfProcessPPAPITest.CharSet',
    'OutOfProcessPPAPITest.Memory',
    'OutOfProcessPPAPITest.VideoDecoder',
    'NPAPIVisiblePluginTester.ClickToPlay',
    'NPAPIAutomationEnabledTest.LoadAllBlockedPlugins',
    'HistoryTester.ConsiderRedirectAfterGestureAsUserInitiated',

    # control.two$
    'CheckFalseTest.CheckFails']


blacklist_vm = []
