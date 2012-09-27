#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to parse perf data from Chrome Endure test executions, to be graphed.

This script connects via HTTP to a buildbot master in order to scrape and parse
perf data from Chrome Endure tests that have been run.  The perf data is then
stored in local text files to be graphed by the Chrome Endure graphing code.

It is assumed that any Chrome Endure tests that show up on the waterfall have
names that are of the following form:

"endure_<webapp_name>_test <test_name>" (non-Web Page Replay tests)

or

"endure_<webapp_name>_wpr_test <test_name>" (Web Page Replay tests)

For example: "endure_gmail_wpr_test testGmailComposeDiscard"
"""

import getpass
import logging
import optparse
import os
import re
import simplejson
import socket
import sys
import time
import urllib
import urllib2


CHROME_ENDURE_SLAVE_NAMES = [
  'Linux (perf0)',
  'Linux (perf1)',
  'Linux (perf2)',
  'Linux (perf3)',
  'Linux (perf4)',
]

BUILDER_URL_BASE = 'http://build.chromium.org/p/chromium.pyauto/builders/'
LAST_BUILD_NUM_PROCESSED_FILE = os.path.join(os.path.dirname(__file__),
                                             '_parser_last_processed.txt')
LOCAL_GRAPH_DIR = '/home/%s/www/chrome_endure_clean' % getpass.getuser()


def SetupBaseGraphDirIfNeeded(webapp_name, test_name, dest_dir):
  """Sets up the directory containing results for a particular test, if needed.

  Args:
    webapp_name: The string name of the webapp associated with the given test.
    test_name: The string name of the test.
    dest_dir: The name of the destination directory that needs to be set up.
  """
  if not os.path.exists(dest_dir):
    os.mkdir(dest_dir)  # Test name directory.
    os.chmod(dest_dir, 0755)

  # Create config file.
  config_file = os.path.join(dest_dir, 'config.js')
  if not os.path.exists(config_file):
    with open(config_file, 'w') as f:
      f.write('var Config = {\n')
      f.write('buildslave: "Chrome Endure Bots",\n')
      f.write('title: "Chrome Endure %s Test: %s",\n' % (webapp_name.upper(),
                                                         test_name))
      f.write('};\n')
    os.chmod(config_file, 0755)

  # Set up symbolic links to the real graphing files.
  link_file = os.path.join(dest_dir, 'index.html')
  if not os.path.exists(link_file):
    os.symlink('../../endure_plotter.html', link_file)
  link_file = os.path.join(dest_dir, 'endure_plotter.js')
  if not os.path.exists(link_file):
    os.symlink('../../endure_plotter.js', link_file)
  link_file = os.path.join(dest_dir, 'js')
  if not os.path.exists(link_file):
    os.symlink('../../js', link_file)


def WriteToDataFile(new_line, existing_lines, revision, data_file):
  """Writes a new entry to an existing perf data file to be graphed.

  If there's an existing line with the same revision number, overwrite its data
  with the new line.  Else, prepend the info for the new revision.

  Args:
    new_line: A dictionary representing perf information for the new entry.
    existing_lines: A list of string lines from the existing perf data file.
    revision: The string revision number associated with the new perf entry.
    data_file: The string name of the perf data file to which to write.
  """
  overwritten = False
  for i, line in enumerate(existing_lines):
    line_dict = simplejson.loads(line)
    if line_dict['rev'] == revision:
      existing_lines[i] = simplejson.dumps(new_line)
      overwritten = True
      break
    elif int(line_dict['rev']) < int(revision):
      break
  if not overwritten:
    existing_lines.insert(0, simplejson.dumps(new_line))

  with open(data_file, 'w') as f:
    f.write('\n'.join(existing_lines))
  os.chmod(data_file, 0755)


def OutputPerfData(revision, graph_name, values, units, units_x, dest_dir,
                   is_stacked=False, stack_order=[]):
  """Outputs perf data to a local text file to be graphed.

  Args:
    revision: The string revision number associated with the perf data.
    graph_name: The string name of the graph on which to plot the data.
    values: A dict which maps a description to a value.  A value is either a
        single data value to be graphed, or a list of 2-tuples
        representing (x, y) points to be graphed for long-running tests.
    units: The string description for the y-axis units on the graph.
    units_x: The string description for the x-axis units on the graph.  Should
        be set to None if the results are not for long-running graphs.
    dest_dir: The name of the destination directory to which to write.
    is_stacked: True to draw a "stacked" graph.  First-come values are
        stacked at bottom by default.
    stack_order: A list that contains key strings in the order to stack values
        in the graph.
  """
  # Update graphs.dat, which contains metadata associated with each graph.
  existing_graphs = []
  graphs_file = os.path.join(dest_dir, 'graphs.dat')
  if os.path.exists(graphs_file):
    with open(graphs_file, 'r') as f:
      existing_graphs = simplejson.loads(f.read())
  is_new_graph = True
  for graph in existing_graphs:
    if graph['name'] == graph_name:
      is_new_graph = False
      break
  if is_new_graph:
    new_graph =  {
      'name': graph_name,
      'units': units,
      'important': False,
    }
    if units_x:
      new_graph['units_x'] = units_x
    existing_graphs.append(new_graph)
    existing_graphs = sorted(existing_graphs, key=lambda x: x['name'])
    with open(graphs_file, 'w') as f:
      f.write(simplejson.dumps(existing_graphs, indent=2))
    os.chmod(graphs_file, 0755)

  # Update summary data file, containing the actual data to be graphed.
  data_file_name = graph_name + '-summary.dat'
  existing_lines = []
  data_file = os.path.join(dest_dir, data_file_name)
  if os.path.exists(data_file):
    with open(data_file, 'r') as f:
      existing_lines = f.readlines()
  existing_lines = map(lambda x: x.strip(), existing_lines)
  new_traces = {}
  for description in values:
    value = values[description]
    if units_x:
      points = []
      for point in value:
        points.append([str(point[0]), str(point[1])])
      new_traces[description] = points
    else:
      new_traces[description] = [str(value), str(0.0)]
  new_line = {
    'traces': new_traces,
    'rev': revision
  }
  if is_stacked:
    new_line['stack'] = True
    new_line['stack_order'] = stack_order

  WriteToDataFile(new_line, existing_lines, revision, data_file)


def OutputEventData(revision, event_dict, dest_dir):
  """Outputs event data to a local text file to be graphed.

  Args:
    revision: The string revision number associated with the event data.
    event_dict: A dict which maps a description to an array of tuples
        representing event data to be graphed.
    dest_dir: The name of the destination directory to which to write.
  """
  data_file_name = '_EVENT_-summary.dat'
  existing_lines = []
  data_file = os.path.join(dest_dir, data_file_name)
  if os.path.exists(data_file):
    with open(data_file, 'r') as f:
      existing_lines = f.readlines()
  existing_lines = map(lambda x: x.strip(), existing_lines)

  new_events = {}
  for description in event_dict:
    event_list = event_dict[description]
    value_list = []
    for event_time, event_data in event_list:
      value_list.append([str(event_time), event_data])
    new_events[description] = value_list

  new_line = {
    'rev': revision,
    'events': new_events
  }

  WriteToDataFile(new_line, existing_lines, revision, data_file)


def UpdatePerfDataForSlaveAndBuild(slave_info, build_num):
  """Process updated perf data for a particular slave and build number.

  Args:
    slave_info: A dictionary containing information about the slave to process.
    build_num: The particular build number on the slave to process.

  Returns:
    True if the perf data for the given slave/build is updated properly, or
    False if any critical error occurred.
  """
  logging.debug('  %s, build %d.', slave_info['slave_name'], build_num)
  build_url = (BUILDER_URL_BASE + urllib.quote(slave_info['slave_name']) +
               '/builds/' + str(build_num))

  url_contents = ''
  fp = None
  try:
    fp = urllib2.urlopen(build_url, timeout=60)
    url_contents = fp.read()
  except urllib2.URLError, e:
    logging.exception('Error reading build URL "%s": %s', build_url, str(e))
    return False
  finally:
    if fp:
      fp.close()

  # Extract the revision number for this build.
  revision = re.findall(
      r'<td class="left">got_revision</td>\s+<td>(\d+)</td>\s+<td>Source</td>',
      url_contents)
  if not revision:
    logging.warning('Could not get revision number. Assuming build is too new '
                    'or was cancelled.')
    return True  # Do not fail the script in this case; continue with next one.
  revision = revision[0]

  # Extract any Chrome Endure stdio links for this build.
  stdio_urls = []
  links = re.findall(r'(/steps/endure[^/]+/logs/stdio)', url_contents)
  for link in links:
    link_unquoted = urllib.unquote(link)
    found_wpr_result = False
    match = re.findall(r'endure_([^_]+)_test ([^/]+)/', link_unquoted)
    if not match:
      match = re.findall(r'endure_([^_]+)_wpr_test ([^/]+)/', link_unquoted)
      if match:
        found_wpr_result = True
      else:
        logging.error('Test name not in expected format in link: ' +
                      link_unquoted)
        return False
    match = match[0]
    webapp_name = match[0] + '_wpr' if found_wpr_result else match[0]
    test_name = match[1]
    stdio_urls.append({
      'link': build_url + link + '/text',
      'webapp_name': webapp_name,
      'test_name': test_name,
    })

  # For each test stdio link, parse it and look for new perf data to be graphed.
  for stdio_url_data in stdio_urls:
    stdio_url = stdio_url_data['link']
    url_contents = ''
    fp = None
    try:
      fp = urllib2.urlopen(stdio_url, timeout=60)
      # Since in-progress test output is sent chunked, there's no EOF.  We need
      # to specially handle this case so we don't hang here waiting for the
      # test to complete.
      start_time = time.time()
      while True:
        data = fp.read(1024)
        if not data:
          break
        url_contents += data
        if time.time() - start_time >= 30:  # Read for at most 30 seconds.
          break
    except (urllib2.URLError, socket.error), e:
      # Issue warning but continue to the next stdio link.
      logging.warning('Error reading test stdio URL "%s": %s', stdio_url,
                      str(e))
    finally:
      if fp:
        fp.close()

    perf_data_raw = []

    def AppendRawPerfData(graph_name, description, value, units, units_x,
                          webapp_name, test_name, is_stacked=False):
      perf_data_raw.append({
        'graph_name': graph_name,
        'description': description,
        'value': value,
        'units': units,
        'units_x': units_x,
        'webapp_name': webapp_name,
        'test_name': test_name,
        'stack': is_stacked,
      })

    # First scan for short-running perf test results.
    for match in re.findall(
        r'RESULT ([^:]+): ([^=]+)= ([-\d\.]+) (\S+)', url_contents):
      AppendRawPerfData(match[0], match[1], eval(match[2]), match[3], None,
                        stdio_url_data['webapp_name'],
                        stdio_url_data['webapp_name'])

    # Next scan for long-running perf test results.
    for match in re.findall(
        r'RESULT ([^:]+): ([^=]+)= (\[[^\]]+\]) (\S+) (\S+)', url_contents):
      # TODO(dmikurube): Change the condition to use stacked graph when we
      # determine how to specify it.
      AppendRawPerfData(match[0], match[1], eval(match[2]), match[3], match[4],
                        stdio_url_data['webapp_name'],
                        stdio_url_data['test_name'],
                        match[0].endswith('-DMP'))

    # Next scan for events in the test results.
    for match in re.findall(
        r'RESULT _EVENT_: ([^=]+)= (\[[^\]]+\])', url_contents):
      AppendRawPerfData('_EVENT_', match[0], eval(match[1]), None, None,
                        stdio_url_data['webapp_name'],
                        stdio_url_data['test_name'])

    # For each graph_name/description pair that refers to a long-running test
    # result or an event, concatenate all the results together (assume results
    # in the input file are in the correct order).  For short-running test
    # results, keep just one if more than one is specified.
    perf_data = {}  # Maps a graph-line key to a perf data dictionary.
    for data in perf_data_raw:
      key_graph = data['graph_name']
      key_description = data['description']
      if not key_graph in perf_data:
        perf_data[key_graph] = {
          'graph_name': data['graph_name'],
          'value': {},
          'units': data['units'],
          'units_x': data['units_x'],
          'webapp_name': data['webapp_name'],
          'test_name': data['test_name'],
        }
      perf_data[key_graph]['stack'] = data['stack']
      if 'stack_order' not in perf_data[key_graph]:
        perf_data[key_graph]['stack_order'] = []
      if (data['stack'] and
          data['description'] not in perf_data[key_graph]['stack_order']):
        perf_data[key_graph]['stack_order'].append(data['description'])

      if data['graph_name'] != '_EVENT_' and not data['units_x']:
        # Short-running test result.
        perf_data[key_graph]['value'][key_description] = data['value']
      else:
        # Long-running test result or event.
        if key_description in perf_data[key_graph]['value']:
          perf_data[key_graph]['value'][key_description] += data['value']
        else:
          perf_data[key_graph]['value'][key_description] = data['value']

    # Finally, for each graph-line in |perf_data|, update the associated local
    # graph data files if necessary.
    for perf_data_key in perf_data:
      perf_data_dict = perf_data[perf_data_key]

      dest_dir = os.path.join(LOCAL_GRAPH_DIR, perf_data_dict['webapp_name'])
      if not os.path.exists(dest_dir):
        os.mkdir(dest_dir)  # Webapp name directory.
        os.chmod(dest_dir, 0755)
      dest_dir = os.path.join(dest_dir, perf_data_dict['test_name'])

      SetupBaseGraphDirIfNeeded(perf_data_dict['webapp_name'],
                                perf_data_dict['test_name'], dest_dir)
      if perf_data_dict['graph_name'] == '_EVENT_':
        OutputEventData(revision, perf_data_dict['value'], dest_dir)
      else:
        OutputPerfData(revision, perf_data_dict['graph_name'],
                       perf_data_dict['value'],
                       perf_data_dict['units'], perf_data_dict['units_x'],
                       dest_dir,
                       perf_data_dict['stack'], perf_data_dict['stack_order'])

  return True


def UpdatePerfDataFiles():
  """Updates the Chrome Endure graph data files with the latest test results.

  For each known Chrome Endure slave, we scan its latest test results looking
  for any new test data.  Any new data that is found is then appended to the
  data files used to display the Chrome Endure graphs.

  Returns:
    True if all graph data files are updated properly, or
    False if any error occurred.
  """
  slave_list = []
  for slave_name in CHROME_ENDURE_SLAVE_NAMES:
    slave_info = {}
    slave_info['slave_name'] = slave_name
    slave_info['most_recent_build_num'] = None
    slave_info['last_processed_build_num'] = None
    slave_list.append(slave_info)

  # Identify the most recent build number for each slave.
  logging.debug('Searching for latest build numbers for each slave...')
  for slave in slave_list:
    slave_name = slave['slave_name']
    slave_url = BUILDER_URL_BASE + urllib.quote(slave_name)

    url_contents = ''
    fp = None
    try:
      fp = urllib2.urlopen(slave_url, timeout=60)
      url_contents = fp.read()
    except urllib2.URLError, e:
      logging.exception('Error reading builder URL: %s', str(e))
      return False
    finally:
      if fp:
        fp.close()

    matches = re.findall(r'/(\d+)/stop', url_contents)
    if matches:
      slave['most_recent_build_num'] = int(matches[0])
    else:
      matches = re.findall(r'#(\d+)</a></td>', url_contents)
      if matches:
        slave['most_recent_build_num'] = sorted(map(int, matches),
                                                reverse=True)[0]
      else:
        logging.error('Could not identify latest build number for slave %s.',
                      slave_name)
        return False

    logging.debug('%s most recent build number: %s', slave_name,
                  slave['most_recent_build_num'])

  # Identify the last-processed build number for each slave.
  logging.debug('Identifying last processed build numbers...')
  if not os.path.exists(LAST_BUILD_NUM_PROCESSED_FILE):
    for slave_info in slave_list:
      slave_info['last_processed_build_num'] = 0
  else:
    with open(LAST_BUILD_NUM_PROCESSED_FILE, 'r') as fp:
      file_contents = fp.read()
      for match in re.findall(r'([^:]+):(\d+)', file_contents):
        slave_name = match[0].strip()
        last_processed_build_num = match[1].strip()
        for slave_info in slave_list:
          if slave_info['slave_name'] == slave_name:
            slave_info['last_processed_build_num'] = int(
                last_processed_build_num)
    for slave_info in slave_list:
      if not slave_info['last_processed_build_num']:
        slave_info['last_processed_build_num'] = 0
  logging.debug('Done identifying last processed build numbers.')

  # For each Chrome Endure slave, process each build in-between the last
  # processed build num and the most recent build num, inclusive.  To process
  # each one, first get the revision number for that build, then scan the test
  # result stdio for any performance data, and add any new performance data to
  # local files to be graphed.
  for slave_info in slave_list:
    logging.debug('Processing %s, builds %d-%d...',
                  slave_info['slave_name'],
                  slave_info['last_processed_build_num'],
                  slave_info['most_recent_build_num'])
    curr_build_num = slave_info['last_processed_build_num']
    while curr_build_num <= slave_info['most_recent_build_num']:
      if not UpdatePerfDataForSlaveAndBuild(slave_info, curr_build_num):
        return False
      curr_build_num += 1

  # Log the newly-processed build numbers.
  logging.debug('Logging the newly-processed build numbers...')
  with open(LAST_BUILD_NUM_PROCESSED_FILE, 'w') as f:
    for slave_info in slave_list:
      f.write('%s:%s\n' % (slave_info['slave_name'],
                           slave_info['most_recent_build_num']))

  return True


def GenerateIndexPage():
  """Generates a summary (landing) page for the Chrome Endure graphs."""
  logging.debug('Generating new index.html page...')

  # Page header.
  page = """
  <html>

  <head>
    <title>Chrome Endure Overview</title>
    <script language="javascript">
      function DisplayGraph(name, graph) {
        document.write(
            '<td><iframe scrolling="no" height="438" width="700" src="');
        document.write(name);
        document.write('"></iframe></td>');
      }
    </script>
  </head>

  <body>
    <center>

      <h1>
        Chrome Endure
      </h1>
  """
  # Print current time.
  page += '<p>Updated: %s</p>\n' % (
      time.strftime('%A, %B %d, %Y at %I:%M:%S %p %Z'))

  # Links for each webapp.
  webapp_names = [x for x in os.listdir(LOCAL_GRAPH_DIR) if
                  x not in ['js', 'old_data'] and
                  os.path.isdir(os.path.join(LOCAL_GRAPH_DIR, x))]
  webapp_names = sorted(webapp_names)

  page += '<p> ['
  for i, name in enumerate(webapp_names):
    page += '<a href="#%s">%s</a>' % (name.upper(), name.upper())
    if i < len(webapp_names) - 1:
      page += ' | '
  page += '] </p>\n'

  # Print out the data for each webapp.
  for webapp_name in webapp_names:
    page += '\n<h1 id="%s">%s</h1>\n' % (webapp_name.upper(),
                                         webapp_name.upper())

    # Links for each test for this webapp.
    test_names = [x for x in
                  os.listdir(os.path.join(LOCAL_GRAPH_DIR, webapp_name))]
    test_names = sorted(test_names)

    page += '<p> ['
    for i, name in enumerate(test_names):
      page += '<a href="#%s">%s</a>' % (name, name)
      if i < len(test_names) - 1:
        page += ' | '
    page += '] </p>\n'

    # Print out the data for each test for this webapp.
    for test_name in test_names:
      # Get the set of graph names for this test.
      graph_names = [x[:x.find('-summary.dat')] for x in
                     os.listdir(os.path.join(LOCAL_GRAPH_DIR,
                                             webapp_name, test_name))
                     if '-summary.dat' in x and '_EVENT_' not in x]
      graph_names = sorted(graph_names)

      page += '<h2 id="%s">%s</h2>\n' % (test_name, test_name)
      page += '<table>\n'

      for i, graph_name in enumerate(graph_names):
        if i % 2 == 0:
          page += '  <tr>\n'
        page += ('    <script>DisplayGraph("%s/%s?graph=%s&lookout=1");'
                 '</script>\n' % (webapp_name, test_name, graph_name))
        if i % 2 == 1:
          page += '  </tr>\n'
      if len(graph_names) % 2 == 1:
        page += '  </tr>\n'
      page += '</table>\n'

  # Page footer.
  page += """
    </center>
  </body>

  </html>
  """

  index_file = os.path.join(LOCAL_GRAPH_DIR, 'index.html')
  with open(index_file, 'w')  as f:
    f.write(page)
  os.chmod(index_file, 0755)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-v', '--verbose', action='store_true', default=False,
      help='Use verbose logging.')
  options, _ = parser.parse_args(sys.argv)

  logging_level = logging.DEBUG if options.verbose else logging.INFO
  logging.basicConfig(level=logging_level,
                      format='[%(asctime)s] %(levelname)s: %(message)s')

  success = UpdatePerfDataFiles()
  if not success:
    logging.error('Failed to update perf data files.')
    sys.exit(0)

  GenerateIndexPage()
  logging.debug('All done!')


if __name__ == '__main__':
  main()
