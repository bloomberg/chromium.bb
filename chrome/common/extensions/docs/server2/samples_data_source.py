# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import re

DEFAULT_ICON_PATH = '/static/images/sample-default-icon.png'

class SamplesDataSource(object):
  """Constructs a list of samples and their respective files and api calls.
  """
  def __init__(self, fetcher, cache_builder, samples_path):
    self._fetcher = fetcher
    self._cache = cache_builder.build(self._MakeSamplesList)
    self._samples_path = samples_path

  def _GetApiItems(self, js_file):
    return set(re.findall('(chrome\.[a-zA-Z0-9\.]+)', js_file))

  def _MakeApiLink(self, prefix, item):
    api, name = item.replace('chrome.', '').split('.', 1)
    return api + '.html#' + prefix + '-' + name

  def _GetDataFromManifest(self, path):
    manifest_path = path + '/manifest.json'
    manifest = self._fetcher.Read([manifest_path]).Get()[manifest_path]
    manifest_json = json.loads(manifest)
    return {
      'name': manifest_json.get('name'),
      'description': manifest_json.get('description'),
      'icon': manifest_json.get('icons', {}).get('128', None)
    }

  def _MakeSamplesList(self, files):
    samples_list = []
    for filename in sorted(files):
      if filename.rsplit('/')[-1] != 'manifest.json':
        continue
      # This is a little hacky, but it makes a sample page.
      sample_path = filename.rsplit('/', 1)[-2]
      sample_files = filter(lambda x: x.startswith(sample_path + '/'), files)
      js_files = filter(lambda x: x.endswith('.js'), sample_files)
      js_contents = self._fetcher.Read(js_files).Get()
      api_items = set()
      for js in js_contents.values():
        api_items.update(self._GetApiItems(js))

      api_calls = []
      for item in api_items:
        if len(item.split('.')) < 3:
          continue
        if item.endswith('.addListener'):
          item = item.replace('.addListener', '')
          api_calls.append({
            'name': item,
            'link': self._MakeApiLink('event', item)
          })
        else:
          api_calls.append({
            'name': item,
            'link': self._MakeApiLink('method', item)
          })
      samples_info = self._GetDataFromManifest(sample_path)
      sample_base_path = sample_path.split('/', 1)[1]
      if samples_info['icon'] is None:
        icon_path = DEFAULT_ICON_PATH
      else:
        icon_path = sample_base_path + '/' + samples_info['icon']
      samples_info.update({
        'icon': icon_path,
        'path': sample_base_path,
        'files': [f.replace(sample_path + '/', '') for f in sample_files],
        'api_calls': api_calls
      })
      samples_list.append(samples_info)
    return samples_list

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    return self._cache.GetFromFileListing(self._samples_path + '/')
