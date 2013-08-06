# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

# TODO(sukolsak): This should read the uninstall command from the registry and
# run that instead.
subprocess.call('mini_installer.exe --uninstall --multi-install --chrome',
                shell=True)
