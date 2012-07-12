# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import re

class SamplesDataSource(object):
  """Constructs a list of samples and their respective files and api calls.
  """
  def __init__(self, fetcher, cache_builder, samples_path):
    self._fetcher = fetcher
    self._cache = cache_builder.build(self._MakeSamplesList)
    self._samples_path = samples_path

  def _GetApiItems(self, api_items, js_file):
    return set(re.findall('(chrome\.[a-zA-Z0-9\.]+)',
                          self._fetcher.FetchResource(js_file).content))

  def _MakeApiLink(self, prefix, item):
    api, name = item.replace('chrome.', '').split('.', 1)
    return api + '.html#' + prefix + '-' + name

  def _GetDataFromManifest(self, path):
    manifest = self._fetcher.FetchResource(path + '/manifest.json').content
    manifest_json = json.loads(manifest)
    return (manifest_json.get('name'), manifest_json.get('description'))

  def _MakeSamplesList(self, files):
    samples_list = []
    for filename in files:
      if filename.rsplit('/')[-1] != 'manifest.json':
        continue
      # This is a little hacky, but it makes a sample page.
      sample_path = filename.rsplit('/', 1)[-2]
      sample_files = filter(lambda x: x.startswith(sample_path + '/'), files)
      api_items = set([])
      for file_ in sample_files:
        if file_.endswith('.js'):
          api_items.update(self._GetApiItems(api_items, file_))

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
      name, description = self._GetDataFromManifest(sample_path)
      samples_list.append({
        'name': name,
        'description': description,
        'path': sample_path.split('/', 1)[1],
        'files': [f.replace(sample_path + '/', '') for f in sample_files],
        'api_calls': api_calls
      })
    return samples_list

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    return self._cache.getFromFileListing('docs/' + self._samples_path, True)
