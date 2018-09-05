# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

licenses(["notice"])  # zlib, portions BSD, MIT, PostgreSQL

# We are currently using the system SDL2.  Building this on Linux is somewhat
# complicated because it actually expects us to run --configure (i.e. patching
# that in would require to make it work).
cc_library(
    name = "sdl2",
    hdrs = glob([
        "include/*.h",
    ]),
    includes = ["include"],
    linkopts = ["-lSDL2"],
    visibility = ["//visibility:public"],
    alwayslink = 1,
)

# Old attempt at building SDL directly.  May be useful for Windows and OS X
# builds, when those exist.
# cc_library(
#     name = "sdl2",
#     srcs = glob(
#         include = [
#             "src/**/*.c",
#             "src/**/*.h",
#         ],
#         exclude = [
#             "src/video/qnx/**",
#             "src/haptic/windows/**",
#             "src/test/*.c",
#             #"src/thread/**/*.c",
#         ],
#     ),
#     hdrs = glob([
#         "include/*.h",
#     ]),
#     includes = ["include"],
#     copts = [
#         "-DSDL_VIDEO_DRIVER_X11",
#         "-DSDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS",
#         "-DSDL_VIDEO_DRIVER_X11_XDBE",
#         "-DSDL_VIDEO_OPENGL",
#         "-DSDL_VIDEO_OPENGL_GLX",
#         "-DSDL_VIDEO_RENDER_OGL",
#     ],
#     linkopts = [
#         "-ldl",
#         "-lX11",
#         "-lXext",
#     ],
#     visibility = ["//visibility:public"],
# )
