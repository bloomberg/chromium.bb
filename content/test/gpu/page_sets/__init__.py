# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

from telemetry.core import discover
from telemetry.story import story_set

from page_sets.gpu_process_tests import GpuProcessTestsStorySet
from page_sets.gpu_rasterization_tests import GpuRasterizationTestsStorySet
from page_sets.memory_tests import MemoryTestsStorySet
from page_sets.pixel_tests import PixelTestsStorySet
