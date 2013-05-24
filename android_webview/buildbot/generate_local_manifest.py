#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate local manifest in an Android repository.

This is used to generate a local manifest in an Android repository. The purpose
of the generated manifest is to remove the set of projects that exist under a
certain path.
"""

from optparse import OptionParser
import os
import xml.etree.ElementTree as ET

def createLocalManifest(manifest_path, local_manifest_path, path_to_exclude,
                        pinned_projects=None):
  manifest_tree = ET.parse(manifest_path)
  local_manifest_root = ET.Element('manifest')

  def remove_project(project):
    remove_project = ET.SubElement(local_manifest_root, 'remove-project')
    remove_project.set('name', project.get('name'))

  def pin_project(project, revision):
    pin_project = ET.SubElement(local_manifest_root, 'project')
    pin_project.set('name', project.get('name'))
    if project.get('path') != None:
      pin_project.set('path', project.get('path'))
    pin_project.set('revision', revision)

  for project in manifest_tree.getroot().findall('project'):
    project_path = project.get('path')
    project_name = project.get('name')
    exclude_project = ((project_path != None and
                        project_path.startswith(path_to_exclude)) or
                       (project_path == None and
                        project_name.startswith(path_to_exclude)))
    if exclude_project:
      print 'Excluding project name="%s" path="%s"' % (project_name,
                                                       project_path)
      remove_project(project)
      continue

    pinned_projects = pinned_projects or []
    for pinned in pinned_projects:
      if pinned['path'] == project_path and pinned['name'] == project_name:
        remove_project(project)
        pin_project(project, pinned['revision'])
        break

  local_manifest_tree = ET.ElementTree(local_manifest_root)
  local_manifest_dir = os.path.dirname(local_manifest_path)
  if not os.path.exists(local_manifest_dir):
    os.makedirs(local_manifest_dir)
  local_manifest_tree.write(local_manifest_path,
                            xml_declaration=True,
                            encoding='UTF-8',
                            method='xml')

def main():
  usage = 'usage: %prog [options] <android_build_top> <path_to_exclude>'
  parser = OptionParser(usage=usage)
  parser.add_option('--ndk-revision', dest='ndk_revision',
                    help='pin the ndk project at a particular REVISION',
                    metavar='REVISION', default=None)
  parser.add_option('--manifest_filename', dest='manifest_filename',
                    help='name of the manifest file', default='default.xml')
  (options, args) = parser.parse_args()

  if len(args) != 2:
    parser.error('Wrong number of arguments.')

  android_build_top = args[0]
  path_to_exclude = args[1]

  manifest_filename = options.manifest_filename

  manifest_path = os.path.join(android_build_top, '.repo/manifests',
                               manifest_filename)
  local_manifest_path = os.path.join(android_build_top,
                                     '.repo/local_manifest.xml')

  pinned_projects = []
  if options.ndk_revision:
    pinned_projects = [{
        'path': 'ndk',
        'name': 'platform/ndk',
        'revision' : options.ndk_revision,
    },]

  print 'Path to exclude: %s' % path_to_exclude
  print 'Path to manifest file: %s' % manifest_path
  createLocalManifest(manifest_path, local_manifest_path, path_to_exclude,
                      pinned_projects)
  print 'Local manifest created in: %s' % local_manifest_path

if __name__ == '__main__':
  main()
