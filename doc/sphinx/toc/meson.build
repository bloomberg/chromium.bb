# you need to add here any files you add to the toc directory as well
files = [ 'libweston.rst' ]

foreach file : files
	configure_file(input: file, output: file, copy: true)
endforeach

subdir('libweston')
