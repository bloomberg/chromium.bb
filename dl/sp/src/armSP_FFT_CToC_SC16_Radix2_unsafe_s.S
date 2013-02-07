;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix2_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   5892
;// Last Modified Date:       Thu, 07 Jun 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 2 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)

           
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



;// Guarding implementation by the processor name
    
    
    
    
    ;// Guarding implementation by the processor name
    
    IF  CortexA8 
    
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7


;//Output Registers


;//Local Scratch Registers

outPointStep    RN  3
pointStep       RN  4
grpCount        RN  5
setCount        RN  8
step            RN  10
dstStep         RN  11
pTmp            RN  9    

;// Neon Registers

dW              DN  D0.S16
dX0             DN  D2.S16
dX1                DN  D3.S16
dX2             DN  D4.S16
dX3                DN  D5.S16
dY0             DN  D6.S16
dY1               DN  D7.S16
dY2             DN  D8.S16
dY3               DN  D9.S16
qT0             QN  Q3.S32
qT1             QN  Q4.S32

    
    
        MACRO
        FFTSTAGE $scaled, $inverse, $name
        
        ;// Define stack arguments
        
        
        ;// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs
        
        LSR     subFFTNum,subFFTNum,#1                      ;//grpSize
        LSL     grpCount,subFFTSize,#1
        
        
        ;// pT0+1 increments pT0 by 8 bytes
        ;// pT0+pointStep = increment of 4*pointStep bytes = 2*grpSize bytes
        MOV     pointStep,subFFTNum,LSL #1
        
        ;// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        
        ;// pOut0+1 increments pOut0 by 8 bytes
        ;// pOut0+outPointStep == increment of 4*outPointStep bytes = 2*size bytes
        SMULBB  outPointStep,grpCount,pointStep  
        LSL     pointStep,pointStep,#1    
                               
        
        RSB      step,pointStep,#16
        RSB      dstStep,outPointStep,#16
        
        ;// Loop on the groups

grpLoop$name        
        
        VLD1     dW,[pTwiddle],pointStep                ;//[wi | wr]
        MOV      setCount,pointStep,LSR #2
        
        
        ;// Loop on the sets: 4 at a time
        
        
setLoop$name        
        
        
        VLD2    {dX0,dX1},[pSrc],pointStep            ;// point0: dX0-real part dX1-img part
        VLD2    {dX2,dX3},[pSrc],step                 ;// point1: dX2-real part dX3-img part
        
        SUBS    setCount,setCount,#4               
        
        IF  $inverse
            VMULL   qT0,dX2,dW[0]
            VMLAL   qT0,dX3,dW[1]                       ;// real part
            VMULL   qT1,dX3,dW[0]
            VMLSL   qT1,dX2,dW[1]                       ;// imag part
                
        ELSE
        
            VMULL   qT0,dX2,dW[0]
            VMLSL   qT0,dX3,dW[1]                       ;// real part
            VMULL   qT1,dX3,dW[0]
            VMLAL   qT1,dX2,dW[1]                       ;// imag part
                    
        ENDIF
        
        VRSHRN  dX2,qT0,#15
        VRSHRN  dX3,qT1,#15
        
        IF $scaled
            VHSUB    dY0,dX0,dX2
            VHSUB    dY1,dX1,dX3
            VHADD    dY2,dX0,dX2
            VHADD    dY3,dX1,dX3
                
        ELSE
            VSUB    dY0,dX0,dX2
            VSUB    dY1,dX1,dX3
            VADD    dY2,dX0,dX2
            VADD    dY3,dX1,dX3
        
        ENDIF
        
        VST2    {dY0,dY1},[pDst],outPointStep
        VST2    {dY2,dY3},[pDst],dstStep              ;// dstStep = -outPointStep + 16
        
        BGT     setLoop$name
        
        SUBS    grpCount,grpCount,#2               
        ADD     pSrc,pSrc,pointStep
        BGT     grpLoop$name    
        
        
        ;// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       ;// pDst -= 2*size; pSrc -= 4*size bytes           
        SUB     pSrc,pTmp,outPointStep
        
        ;// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      ;// pTwiddle -= 2*size bytes
        
                
        MEND
        
        
        
        M_START armSP_FFTFwd_CToC_SC16_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        

    ENDIF                 ;//CORTEXA8
        
    
     
    END    
     
