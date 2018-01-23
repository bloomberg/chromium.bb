# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a archive manifest used for Fuchsia package generation.

Arguments:
  root_dir: The absolute path to the Chromium source tree root.

  out_dir: The absolute path to the Chromium build directory.

  app_name: The filename of the package's executable target.

  runtime_deps: The path to the GN runtime deps file.

  output_path: The path of the manifest file which will be written.
"""

import json
import os
import sys
import tempfile


# Path to file describing the services to be made available to the process.
SANDBOX_POLICY_PATH = 'build/config/fuchsia/sandbox_policy'


def MakePackagePath(file_path, roots):
  """Computes a path for |file_path| that is relative to one of the directory
  paths in |roots|.

  file_path: The absolute file path to relativize.
  roots: A list of absolute directory paths which may serve as a relative root
         for |file_path|. At least one path must contain |file_path|.
         Overlapping roots are permitted; the deepest matching root will be
         chosen.

  Examples:

  >>> MakePackagePath('/foo/bar.txt', ['/foo/'])
  'bar.txt'

  >>> MakePackagePath('/foo/dir/bar.txt', ['/foo/'])
  'dir/bar.txt'

  >>> MakePackagePath('/foo/out/Debug/bar.exe', ['/foo/', '/foo/out/Debug/'])
  'bar.exe'
  """

  # Prevents greedily matching against a shallow path when a deeper, better
  # matching path exists.
  roots.sort(key=len, reverse=True)

  for next_root in roots:
    if not next_root.endswith(os.sep):
      next_root += os.sep

    if file_path.startswith(next_root):
      relative_path = file_path[len(next_root):]

      # TODO(fuchsia): The requirements for finding/loading .so are in flux, so
      # this ought to be reconsidered at some point.
      # See https://crbug.com/732897.
      if file_path.endswith('.so'):
        relative_path = 'lib/' + os.path.basename(relative_path)

      return relative_path

  sys.stderr.write(
      'Error: no matching root paths found for \'%s\'.' % file_path)
  assert False


def BuildManifest(root_dir, out_dir, app_name, runtime_deps_file, output_path):
  with open(output_path, "w") as output:
    # Process the runtime deps file for file paths, recursively walking
    # directories as needed.
    # runtime_deps may contain duplicate paths, so use a set for
    # de-duplication.
    expanded_files = set()
    for next_path in open(runtime_deps_file, 'r'):
      next_path = next_path.strip()
      if os.path.isdir(next_path):
        for root, _, files in os.walk(next_path):
          for next_file in files:
            expanded_files.add(os.path.abspath(os.path.join(root, next_file)))
      else:
        expanded_files.add(os.path.abspath(next_path))

    # Format and write out the manifest contents.
    app_found = False
    for next_file in expanded_files:
      in_package_path = MakePackagePath(os.path.join(out_dir, next_file),
                                        [root_dir, out_dir])
      if in_package_path == app_name:
        in_package_path = 'bin/app'
        app_found = True
      output.write('%s=%s\n' % (in_package_path, next_file))
    assert app_found
    output.write('meta/sandbox=%s%s' % (root_dir, SANDBOX_POLICY_PATH))

  return 0


if __name__ == '__main__':
  sys.exit(BuildManifest(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4],
                         sys.argv[5]))
