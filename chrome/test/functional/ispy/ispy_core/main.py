# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import webapp2

from handlers import command_handler
from handlers import debug_view_handler
from handlers import image_handler
from handlers import main_view_handler
from handlers import update_mask_handler

application = webapp2.WSGIApplication(
    [('/run_command?', command_handler.CommandHandler),
     ('/update_mask?', update_mask_handler.UpdateMaskHandler),
     ('/debug_view?', debug_view_handler.DebugViewHandler),
     ('/image?', image_handler.ImageHandler),
     ('/', main_view_handler.MainViewHandler)], debug=True)
