{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'sine_synth',
      'TYPE' : 'main',
      'SOURCES' : ['sine_synth.cc']
    }
  ],
  'DEST': 'examples',
  'NAME': 'sine_synth',
  'TITLE': 'Sine Wave Synthesizer',
  'DESC': """
The Sine Wave Synthesizer example demonstrates playing sound (a sine
wave).  Enter the desired frequency and hit play to start, stop to end. The
frequency box will display "Loading, please wait." while the module loads.""",
  'INFO': 'Audio.'
}

