#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Takes the JSON files in components/domain_reliability/baked_in_configs and
encodes their contents as an array of C strings that gets compiled in to Chrome
and loaded at runtime."""


import ast
import json
import os
import sys


# A whitelist of domains that the script will accept when baking configs in to
# Chrome, to ensure incorrect ones are not added accidentally. Subdomains of
# whitelist entries are also allowed (e.g. maps.google.com, ssl.gstatic.com).
DOMAIN_WHITELIST = (
  'admob.com',
  'doubleclick.net',
  'ggpht.com',
  'google-analytics.com',
  'google-syndication.com',
  'google.ac',
  'google.ad',
  'google.ae',
  'google.af',
  'google.ag',
  'google.al',
  'google.am',
  'google.as',
  'google.at',
  'google.az',
  'google.ba',
  'google.be',
  'google.bf',
  'google.bg',
  'google.bi',
  'google.bj',
  'google.bs',
  'google.bt',
  'google.by',
  'google.ca',
  'google.cat',
  'google.cc',
  'google.cd',
  'google.cf',
  'google.cg',
  'google.ch',
  'google.ci',
  'google.cl',
  'google.cm',
  'google.cn',
  'google.co.ao',
  'google.co.bw',
  'google.co.ck',
  'google.co.cr',
  'google.co.hu',
  'google.co.id',
  'google.co.il',
  'google.co.im',
  'google.co.in',
  'google.co.je',
  'google.co.jp',
  'google.co.ke',
  'google.co.kr',
  'google.co.ls',
  'google.co.ma',
  'google.co.mz',
  'google.co.nz',
  'google.co.th',
  'google.co.tz',
  'google.co.ug',
  'google.co.uk',
  'google.co.uz',
  'google.co.ve',
  'google.co.vi',
  'google.co.za',
  'google.co.zm',
  'google.co.zw',
  'google.com',
  'google.com.af',
  'google.com.ag',
  'google.com.ai',
  'google.com.ar',
  'google.com.au',
  'google.com.bd',
  'google.com.bh',
  'google.com.bn',
  'google.com.bo',
  'google.com.br',
  'google.com.by',
  'google.com.bz',
  'google.com.cn',
  'google.com.co',
  'google.com.cu',
  'google.com.cy',
  'google.com.do',
  'google.com.ec',
  'google.com.eg',
  'google.com.et',
  'google.com.fj',
  'google.com.ge',
  'google.com.gh',
  'google.com.gi',
  'google.com.gr',
  'google.com.gt',
  'google.com.hk',
  'google.com.iq',
  'google.com.jm',
  'google.com.jo',
  'google.com.kh',
  'google.com.kw',
  'google.com.lb',
  'google.com.ly',
  'google.com.mm',
  'google.com.mt',
  'google.com.mx',
  'google.com.my',
  'google.com.na',
  'google.com.nf',
  'google.com.ng',
  'google.com.ni',
  'google.com.np',
  'google.com.nr',
  'google.com.om',
  'google.com.pa',
  'google.com.pe',
  'google.com.pg',
  'google.com.ph',
  'google.com.pk',
  'google.com.pl',
  'google.com.pr',
  'google.com.py',
  'google.com.qa',
  'google.com.ru',
  'google.com.sa',
  'google.com.sb',
  'google.com.sg',
  'google.com.sl',
  'google.com.sv',
  'google.com.tj',
  'google.com.tn',
  'google.com.tr',
  'google.com.tw',
  'google.com.ua',
  'google.com.uy',
  'google.com.vc',
  'google.com.ve',
  'google.com.vn',
  'google.cv',
  'google.cz',
  'google.de',
  'google.dj',
  'google.dk',
  'google.dm',
  'google.dz',
  'google.ee',
  'google.es',
  'google.fi',
  'google.fm',
  'google.fr',
  'google.ga',
  'google.ge',
  'google.gg',
  'google.gl',
  'google.gm',
  'google.gp',
  'google.gr',
  'google.gy',
  'google.hk',
  'google.hn',
  'google.hr',
  'google.ht',
  'google.hu',
  'google.ie',
  'google.im',
  'google.info',
  'google.iq',
  'google.ir',
  'google.is',
  'google.it',
  'google.it.ao',
  'google.je',
  'google.jo',
  'google.jobs',
  'google.jp',
  'google.kg',
  'google.ki',
  'google.kz',
  'google.la',
  'google.li',
  'google.lk',
  'google.lt',
  'google.lu',
  'google.lv',
  'google.md',
  'google.me',
  'google.mg',
  'google.mk',
  'google.ml',
  'google.mn',
  'google.ms',
  'google.mu',
  'google.mv',
  'google.mw',
  'google.ne',
  'google.ne.jp',
  'google.net',
  'google.ng',
  'google.nl',
  'google.no',
  'google.nr',
  'google.nu',
  'google.off.ai',
  'google.org',
  'google.pk',
  'google.pl',
  'google.pn',
  'google.ps',
  'google.pt',
  'google.ro',
  'google.rs',
  'google.ru',
  'google.rw',
  'google.sc',
  'google.se',
  'google.sh',
  'google.si',
  'google.sk',
  'google.sm',
  'google.sn',
  'google.so',
  'google.sr',
  'google.st',
  'google.td',
  'google.tg',
  'google.tk',
  'google.tl',
  'google.tm',
  'google.tn',
  'google.to',
  'google.tt',
  'google.us',
  'google.uz',
  'google.vg',
  'google.vu',
  'google.ws',
  'googleadservices.com',
  'googlealumni.com',
  'googleapis.com',
  'googleapps.com',
  'googlecbs.com',
  'googlecommerce.com',
  'googledrive.com',
  'googleenterprise.com',
  'googlegoro.com',
  'googlehosted.com',
  'googlepayments.com',
  'googlesource.com',
  'googlesyndication.com',
  'googletagmanager.com',
  'googletagservices.com',
  'googleusercontent.com',
  'googlevideo.com',
  'gstatic.com',
  'gvt1.com',
  's0.2mdn.net',
  'withgoogle.com',
  'youtube.com',
  'ytimg.com'
)


CC_HEADER = """// Copyright (C) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AUTOGENERATED FILE. DO NOT EDIT.
//
// (Update configs in components/domain_reliability/baked_in_configs and list
// configs in components/domain_reliability.gypi instead.)

#include "components/domain_reliability/baked_in_configs.h"

#include <stdlib.h>

namespace domain_reliability {

const char* const kBakedInJsonConfigs[] = {
"""


CC_FOOTER = """  NULL
};

}  // namespace domain_reliability
"""


def get_child(node, children_key, child_key, child_value):
  children = node[children_key]
  for child in children:
    if child[child_key] == child_value:
      return child
  raise KeyError("Couldn't find child with %s = %s in %s" %
                 (child_key, child_value, children_key))


def read_json_files_from_gypi(gypi_file):
  with open(gypi_file, 'r') as f:
    gypi_text = f.read()
  gypi_ast = ast.literal_eval(gypi_text)
  target = get_child(gypi_ast, 'targets', 'target_name', 'domain_reliability')
  action = get_child(target, 'actions', 'action_name', 'bake_in_configs')
  json_files = action['variables']['baked_in_configs']
  gypi_path = os.path.dirname(gypi_file)
  return [ os.path.join(gypi_path, f) for f in json_files ]


def domain_is_whitelisted(domain):
  return any(domain == e or domain.endswith('.' + e)  for e in DOMAIN_WHITELIST)


def quote_and_wrap_text(text, width=79, prefix='  "', suffix='"'):
  max_length = width - len(prefix) - len(suffix)
  output = prefix
  line_length = 0
  for c in text:
    if c == "\"":
      c = "\\\""
    elif c == "\n":
      c = "\\n"
    elif c == "\\":
      c = "\\\\"
    if line_length + len(c) > max_length:
      output += suffix + "\n" + prefix
      line_length = 0
    output += c
    line_length += len(c)
  output += suffix
  return output


def main():
  if len(sys.argv) != 3:
    print >> sys.stderr, ('Usage: %s <input .gypi file> <output C++ file>' %
                          sys.argv[0])
    print >> sys.stderr, sys.modules[__name__].__doc__
    return 1
  gypi_file = sys.argv[1]
  cpp_file = sys.argv[2]

  json_files = read_json_files_from_gypi(gypi_file)

  cpp_code = CC_HEADER
  found_invalid_config = False
  for json_file in json_files:
    with open(json_file, 'r') as f:
      json_text = f.read()
    try:
      config = json.loads(json_text)
    except ValueError:
      print >> sys.stderr, "%s: error parsing JSON" % json_file
      raise
    if 'monitored_domain' not in config:
      print >> sys.stderr, '%s: no monitored_domain found' % json_file
      found_invalid_config = True
      continue
    domain = config['monitored_domain']
    if not domain_is_whitelisted(domain):
      print >> sys.stderr, ('%s: monitored_domain "%s" not in whitelist' %
                            (json_file, domain))
      found_invalid_config = True
      continue
    cpp_code += "  // " + json_file + ":\n"
    cpp_code += quote_and_wrap_text(json_text) + ",\n"
    cpp_code += "\n"
  cpp_code += CC_FOOTER

  if found_invalid_config:
    return 1

  with open(cpp_file, 'wb') as f:
    f.write(cpp_code)

  return 0


if __name__ == '__main__':
  sys.exit(main())
