import subprocess as sub
import re
import sys
import math, functools

class SiteTimes(object):
    def __init__(self):
        self.site='UNDEFINED'
        self.times=[]

# helper function found online here:
# http://code.activestate.com/recipes/511478-finding-the-percentile-
# of-the-values/
def percentile(N, percent, key=lambda x:x):
    """
    Find the percentile of a list of values.

    @parameter N - is a list of values. Note N MUST BE already sorted.
    @parameter percent - a float value from 0.0 to 1.0.
    @parameter key - optional key function to compute value from each 
    element of N.

    @return - the percentile of the values
    """
    if not N:
        return None
    k = (len(N)-1) * percent
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return key(N[int(k)])
    d0 = key(N[int(f)]) * (k-f)
    d1 = key(N[int(c)]) * (c-k)
    return d0+d1

def mean(numbers):
    assert(len(numbers) != 0), 'list should not be empty!'
    return sum(numbers)/len(numbers)

class PageCyclerResultsParser:
    def parse_file(self, outfile = 'out.txt'):
        # output is the output of the page_cycler tests.
        output = open(outfile).read()
        return self.parse_results(output)

    def parse_results(self, output = ''):
        # median is 50th percentile.
        median = functools.partial(percentile, percent=0.5)

        assert(output != ''), 'Output cannot be empty!'

        # split it up into lines
        lines = output.split('\n')

        # figure out where the results are...
        found = False
        # This is our anchor in the text
        token = '*RESULT times:'
        for index, line in enumerate(lines):
            if(line.startswith(token)):
                found = True
                break

        assert(found==True), token+' not found!?'
        timesline = lines[index]
        sitesline = lines[index-1]

        # we have a line called times and a line called sites
        m = re.search('\[(.*?)\]', sitesline)
        sites = m.group(1).split(',')

        m = re.search('\[(.*?)\]', timesline)
        times = m.group(1).split(',')

        assert(len(times) % len(sites) == 0), 'Times not divisible by sites!'

        iterations = len(times)/len(sites)

        # now we have a list called sites and a list called times
        # let's do some statistics on it.
        stList = []

        # go over all the sites and populate the stlist data structure
        for ii, site in enumerate(sites):
            st = SiteTimes()
            st.site = site
            for jj in range(0, iterations):
                mytime = float(times[jj*len(sites)+ii])
                st.times.append(mytime)
            stList.append(st)

        # For debugging use something like this:
        ###for ii, st in enumerate(stList):
        ###  print st.site
        ###  print st.times

        # now remove the lowest element and print out mean of medians
        medianList = []

        totalTime = 0
        for ii, st in enumerate(stList):
            sortedTimes=sorted(st.times)
            # drop highest time in the sortedTimes
            sortedTimes.pop()
            # TODO: Perhaps this should be a weighted mean?
            totalTime += mean(sortedTimes)

        return totalTime/len(stList)

# This is how to use this class
###pcrp=PageCyclerResultsParser()
###print pcrp.parse_file('out.txt')
